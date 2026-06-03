#!/usr/bin/env python3
"""
test_rs485_baud.py — verify the firmware can capture + decode UART frames on the
RS485 channel across a baud-rate / frame-format matrix.

What it checks
==============
The firmware is a logic analyzer: it samples the RS485 line as digital
channel 12 (D12) and streams raw samples over the srpico protocol. "Supporting"
a baud rate therefore means two independent things, both verified here:

  1. CAPABILITY — the firmware accepts the sample rate needed to resolve that
     baud (>= OVERSAMPLE x baud, clamped to the 5 kHz .. 120 MHz limits) and a
     capture completes without an overflow/abort. Needs only the analyzer port.

  2. DECODE     — a real UART frame sent on the bus at {baud, data bits, parity,
     stop bits} is captured and decoded back to the original bytes. Needs a
     second serial port (the transmitter) physically on the RS485 bus.

Matrix
======
Default bauds : 9600, 19200, 57600, 115200, 921600
Default frames: data bits {7,8} x parity {N,E,O} x stop bits {1,2}
Restrict with --bauds / --databits / --parity / --stopbits.

Wiring
======
                 ┌───────────────┐
  TX adapter ────┤ RS485 bus     ├──── analyzer D12 (RS485 channel)
  (--source)     └───────────────┘     (--analyzer, ttyACM)

The transmitter must idle the line HIGH (UART idle) — a plain USB-UART/RS485
adapter does. Half-duplex RS485 transceivers are fine as long as DE is asserted
while sending.

Usage
=====
  # capability-only (no transmitter): does the FW accept the rates?
  python3 tools/test_rs485_baud.py --analyzer /dev/ttyACM0

  # full hardware-in-the-loop decode test
  python3 tools/test_rs485_baud.py --analyzer /dev/ttyACM0 --source /dev/ttyUSB0

  # narrow the matrix
  python3 tools/test_rs485_baud.py -a /dev/ttyACM0 -s /dev/ttyUSB0 \
      --bauds 9600 115200 --databits 8 --parity N E --stopbits 1

Soft USB limit (read the CLAUDE.md note)
========================================
Raw (no-RLE) streaming drains at ~350 KB/s. With D12 enabled every sample is
2 bytes, so sustainable continuous rate is ~175 kS/s. Above that a capture
longer than the ping-pong buffer (1024 samples) overflow-aborts. This script
keeps captures short (a few frames) so high bauds still fit in one buffer, but
921600 at 10x oversample (9.2 MS/s) is near the edge — treat a high-baud
capability FAIL as "needs RLE / shorter capture", not a frame-format bug.

Requires: pip install pyserial
"""

import argparse
import re
import sys
import threading
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial", file=sys.stderr)
    sys.exit(1)

# ── firmware limits (mirror includes/handles/handles_internal.h) ────────────────
SR_MIN = 5_000
SR_MAX = 120_000_000
SIGROK_SAMPLE_LIMIT_MAX = 1_000_000

# RS485 is exposed as digital channel 12 (see merge_digital_sample in
# src/sigrok_handler.c — RS485 line folded in as bit 12).
RS485_CHANNEL = 12

# srpico stream markers
DONE_MARKER_RE = re.compile(rb"\$(\d+)\+")
ABORT_MARKER = b"!!!"
RLE_BASE = 0x30
RLE_MAX = 0x6F
DATA_MIN = 0x80

DEFAULT_BAUDS = [9600, 19200, 57600, 115200, 921600]
DEFAULT_DATABITS = [7, 8]
DEFAULT_PARITY = ["N", "E", "O"]
DEFAULT_STOPBITS = [1, 2]

# Test payloads sent on the bus (decode mode). The 1k payload is a deterministic
# byte ramp so a miss is easy to localize. Both are masked to the frame's data
# bits at compare time, so 7-bit formats still match.
PAYLOADS = {
    "hello": b"Hello World",
    "1k": bytes(i & 0xFF for i in range(1024)),
}

PARITY_MAP = {
    "N": serial.PARITY_NONE,
    "E": serial.PARITY_EVEN,
    "O": serial.PARITY_ODD,
    "M": serial.PARITY_MARK,
    "S": serial.PARITY_SPACE,
}
BYTESIZE_MAP = {5: serial.FIVEBITS, 6: serial.SIXBITS, 7: serial.SEVENBITS, 8: serial.EIGHTBITS}
STOPBITS_MAP = {1: serial.STOPBITS_ONE, 2: serial.STOPBITS_TWO}


# ── srpico analyzer client ──────────────────────────────────────────────────────
class Analyzer:
    """Minimal srpico protocol client, just enough to capture D12."""

    def __init__(self, port: str, timeout: float = 5.0):
        self.ser = serial.Serial(port, baudrate=115200, timeout=timeout)
        time.sleep(0.3)
        self.ser.reset_input_buffer()
        self.n_digital = 13
        self.dig_bps = 2

    def close(self):
        self.ser.close()

    def _send(self, data: bytes):
        self.ser.write(data)
        self.ser.flush()

    def _cmd(self, cmd: str) -> bool:
        """Send a command, wait for the '*' ACK."""
        self._send((cmd + "\n").encode())
        deadline = time.monotonic() + 1.0
        buf = b""
        while time.monotonic() < deadline:
            c = self.ser.read(1)
            if c:
                buf += c
                if buf.endswith(b"*"):
                    return True
        return False

    def reset(self):
        self._send(b"*")
        time.sleep(0.1)
        self.ser.reset_input_buffer()

    def identify(self) -> str:
        self._send(b"i\n")
        deadline = time.monotonic() + 1.5
        resp = b""
        while time.monotonic() < deadline:
            c = self.ser.read(1)
            if c:
                resp += c
                if resp.endswith(b"*"):
                    break
        ident = resp.rstrip(b"*").strip().decode(errors="replace")
        m = re.search(r"A\d{2}\d?D(\d+)", ident)
        if m:
            self.n_digital = int(m.group(1))
            self.dig_bps = 1 if self.n_digital <= 7 else (2 if self.n_digital <= 14 else 3)
        return ident

    def set_rate(self, hz: int) -> bool:
        return self._cmd(f"R{hz}")

    def set_limit(self, n: int) -> bool:
        return self._cmd(f"L{n}")

    def enable_digital(self, ch: int, en: bool = True) -> bool:
        return self._cmd(f"D{1 if en else 0}{ch}")

    def isolate_rs485(self):
        """Enable only D12 so every sample carries the RS485 bit, others off."""
        for ch in range(self.n_digital):
            self.enable_digital(ch, ch == RS485_CHANNEL)

    def capture(self, timeout: float = 8.0):
        """Run a fixed capture, return (channel12_bits, total_sent, aborted)."""
        self.ser.reset_input_buffer()
        self._send(b"F\n")
        raw = self._read_stream(timeout)
        return decode_channel12(raw, self.dig_bps)

    def _read_stream(self, timeout: float) -> bytes:
        buf = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self.ser.read(4096)
            if chunk:
                buf.extend(chunk)
                m = DONE_MARKER_RE.search(bytes(buf))
                if m:
                    return bytes(buf[: m.end()])
                if ABORT_MARKER in buf:
                    return bytes(buf)
        return bytes(buf)


def decode_channel12(raw: bytes, dig_bps: int):
    """Pull the D12 bit out of a raw srpico digital stream.

    Returns (bits, total_sent, aborted). `bits` is a list of 0/1 line states.
    """
    matches = list(DONE_MARKER_RE.finditer(raw))
    total_sent = int(matches[-1].group(1)) if matches else 0
    payload = raw[: matches[-1].start()] if matches else raw

    aborted = False
    if payload.startswith(ABORT_MARKER):
        aborted = True
        payload = payload[len(ABORT_MARKER):]

    bits = []
    i = 0
    n = len(payload)
    while i < n:
        b = payload[i]
        if RLE_BASE <= b <= RLE_MAX:
            count = b - RLE_BASE + 1
            i += 1
        elif b >= DATA_MIN:
            count = 1
        else:
            i += 1
            continue
        if i + dig_bps > n:
            break
        dig_val = 0
        for k in range(dig_bps):
            dig_val |= (payload[i + k] & 0x7F) << (7 * k)
        i += dig_bps
        bit = (dig_val >> RS485_CHANNEL) & 1
        bits.extend([bit] * count)
    return bits, total_sent, aborted


# ── UART decoder (works on the captured line, configurable frame) ───────────────
def decode_uart(bits, samplerate, baud, databits, parity, stopbits):
    """Decode a sampled UART line into bytes.

    bits      : list of 0/1 line states sampled at `samplerate`
    parity    : 'N' | 'E' | 'O' | 'M' | 'S'
    Returns (bytes_out, errors:list[str]).
    """
    spb = samplerate / baud  # samples per bit
    if spb < 3:
        return b"", [f"undersampled: {spb:.1f} samples/bit (need >= 3, ideally >= 10)"]

    out = bytearray()
    errors = []
    n = len(bits)
    i = 0

    def sample_at(pos):
        idx = int(round(pos))
        return bits[idx] if 0 <= idx < n else 1  # idle = high past the end

    while i < n - 1:
        # find a falling edge (idle high -> start bit low)
        if not (bits[i] == 1 and bits[i + 1] == 0):
            i += 1
            continue
        start = i + 1  # first low sample = start bit edge

        # verify start bit is still low at its center
        if sample_at(start + spb * 0.5) != 0:
            i += 1
            continue

        val = 0
        for b in range(databits):
            center = start + spb * (1.5 + b)
            if sample_at(center):
                val |= 1 << b  # LSB first

        bit_idx = databits  # next bit position after data
        parity_ok = True
        if parity != "N":
            pbit = sample_at(start + spb * (1.5 + databits))
            ones = bin(val).count("1")
            if parity == "E":
                expected = ones & 1
            elif parity == "O":
                expected = (ones & 1) ^ 1
            elif parity == "M":
                expected = 1
            else:  # 'S'
                expected = 0
            if pbit != expected:
                parity_ok = False
            bit_idx += 1

        # stop bit(s) must be high
        stop_center = start + spb * (1.5 + bit_idx)
        stop_ok = sample_at(stop_center) == 1
        if stopbits == 2:
            stop_ok = stop_ok and sample_at(stop_center + spb) == 1

        if not parity_ok:
            errors.append(f"parity error at byte {len(out)} (val=0x{val:02X})")
        if not stop_ok:
            errors.append(f"framing error at byte {len(out)} (val=0x{val:02X})")

        out.append(val & 0xFF)
        # advance past the whole frame
        frame_bits = 1 + databits + (1 if parity != "N" else 0) + stopbits
        i = int(start + spb * frame_bits)

    return bytes(out), errors


# ── transmitter thread ──────────────────────────────────────────────────────────
class Transmitter:
    """Repeatedly blast a pattern on the source port while a capture runs."""

    def __init__(self, port, baud, databits, parity, stopbits, pattern):
        self.cfg = dict(
            port=port,
            baudrate=baud,
            bytesize=BYTESIZE_MAP[databits],
            parity=PARITY_MAP[parity],
            stopbits=STOPBITS_MAP[stopbits],
            timeout=1,
        )
        self.pattern = pattern
        self._stop = threading.Event()
        self._thr = None
        self.error = None

    def __enter__(self):
        self._thr = threading.Thread(target=self._run, daemon=True)
        self._thr.start()
        time.sleep(0.05)  # let the line settle / first frame go out
        return self

    def __exit__(self, *exc):
        self._stop.set()
        if self._thr:
            self._thr.join(timeout=2)

    def _run(self):
        try:
            with serial.Serial(**self.cfg) as s:
                while not self._stop.is_set():
                    s.write(self.pattern)
                    s.flush()
        except serial.SerialException as e:
            self.error = str(e)


# ── per-combo test ──────────────────────────────────────────────────────────────
def oversample_rate(baud, oversample):
    rate = baud * oversample
    clamped = max(SR_MIN, min(SR_MAX, rate))
    return clamped, (clamped != rate)


def run_capability(an: Analyzer, baud, oversample, samples):
    rate, clamped = oversample_rate(baud, oversample)
    if not an.set_rate(rate):
        return False, f"rate {rate} rejected"
    an.set_limit(samples)
    bits, total, aborted = an.capture()
    if aborted:
        return False, f"capture aborted/overflow @ {rate} Hz"
    if not bits:
        return False, f"no samples @ {rate} Hz (total_sent={total})"
    spb = rate / baud
    note = f"rate={rate} Hz ({spb:.1f} s/bit, {len(bits)} samp)"
    if clamped:
        note += " [rate clamped]"
    return True, note


def run_decode(an: Analyzer, src_port, baud, databits, parity, stopbits, oversample, payload):
    rate, clamped = oversample_rate(baud, oversample)
    if not an.set_rate(rate):
        return False, f"rate {rate} rejected"

    # UART carries only `databits` bits — mask the expected payload so 7-bit
    # formats compare against what actually goes on the wire.
    mask = (1 << databits) - 1
    expected = bytes(b & mask for b in payload)

    # capture enough samples to hold a full payload plus slack for alignment
    frame_bits = 1 + databits + (1 if parity != "N" else 0) + stopbits
    spb = rate / baud
    samples = int(frame_bits * spb * (len(payload) + 4))
    samples = max(256, min(samples, SIGROK_SAMPLE_LIMIT_MAX))
    an.set_limit(samples)

    # capture window may be long for the 1k payload; scale the read timeout
    cap_timeout = max(8.0, samples / 30_000.0 + 4.0)

    with Transmitter(src_port, baud, databits, parity, stopbits, payload) as tx:
        bits, total, aborted = an.capture(timeout=cap_timeout)
        if tx.error:
            return False, f"source port error: {tx.error}"

    if aborted:
        return False, f"capture aborted/overflow @ {rate} Hz ({samples} samp)"
    if not bits:
        return False, "no samples captured"
    if spb < 3:
        return False, f"undersampled ({spb:.1f} s/bit) — raise --oversample or baud too high"

    decoded, errors = decode_uart(bits, rate, baud, databits, parity, stopbits)

    # the source loops the payload; success = the full payload appears intact
    found = expected in decoded
    if found and not errors:
        return True, f"{len(expected)} B OK ({spb:.1f} s/bit, {samples} samp)"
    if found and errors:
        return True, f"{len(expected)} B OK, {len(errors)} frame/parity warns ({errors[0]})"
    snippet = decoded[:16].hex()
    msg = f"payload not found in {len(decoded)} B decoded (got {snippet}…)"
    if errors:
        msg += f"; {errors[0]}"
    return False, msg


def find_acm() -> str | None:
    for p in serial.tools.list_ports.comports():
        if any(k in p.device for k in ("ACM", "usbmodem")):
            return p.device
    return None


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Verify firmware RS485 baud/frame-format support.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("-a", "--analyzer", help="Analyzer srpico port (ttyACM, auto-detected)")
    ap.add_argument("-s", "--source", help="UART transmitter port on the RS485 bus "
                                           "(omit for capability-only test)")
    ap.add_argument("--bauds", type=int, nargs="+", default=DEFAULT_BAUDS)
    ap.add_argument("--databits", type=int, nargs="+", default=DEFAULT_DATABITS,
                    choices=[5, 6, 7, 8])
    ap.add_argument("--parity", nargs="+", default=DEFAULT_PARITY,
                    choices=["N", "E", "O", "M", "S"])
    ap.add_argument("--stopbits", type=int, nargs="+", default=DEFAULT_STOPBITS,
                    choices=[1, 2])
    ap.add_argument("--oversample", type=int, default=10,
                    help="Samples per bit target = oversample x baud (default 10)")
    ap.add_argument("--samples", type=int, default=2048,
                    help="Capability-mode capture length in samples (default 2048)")
    ap.add_argument("--payloads", nargs="+", default=list(PAYLOADS),
                    choices=list(PAYLOADS),
                    help='Decode-mode payloads to send (default: all -> "Hello World" + 1k ramp)')
    args = ap.parse_args()

    port = args.analyzer or find_acm()
    if not port:
        print("Error: no analyzer port. Pass --analyzer /dev/ttyACM0", file=sys.stderr)
        return 1

    mode = "DECODE (hardware-in-the-loop)" if args.source else "CAPABILITY (rate/capture only)"

    an = Analyzer(port)
    print(f"[analyzer] {port}")
    ident = an.identify()
    print(f"[identify] {ident}")
    print(f"[mode]     {mode}")
    if args.source:
        plist = ", ".join(f"{n} ({len(PAYLOADS[n])} B)" for n in args.payloads)
        print(f"[source]   {args.source}  payloads={plist}")
    print(f"[matrix]   bauds={args.bauds} databits={args.databits} "
          f"parity={args.parity} stopbits={args.stopbits} oversample={args.oversample}x")
    print()

    an.reset()
    an.identify()
    an.isolate_rs485()

    results = []
    header = f"{'baud':>8}  {'fmt':>6}  {'payload':>8}  {'result':<6}  detail"
    print(header)
    print("-" * max(80, len(header)))

    for baud in args.bauds:
        if args.source:
            combos = [
                (d, p, s, name)
                for d in args.databits
                for p in args.parity
                for s in args.stopbits
                for name in args.payloads
            ]
        else:
            combos = [(None, None, None, None)]  # format/payload irrelevant for capability

        for d, p, s, name in combos:
            if args.source:
                ok, detail = run_decode(an, args.source, baud, d, p, s,
                                        args.oversample, PAYLOADS[name])
                fmt = f"{d}{p}{s}"
                pcol = name
            else:
                ok, detail = run_capability(an, baud, args.oversample, args.samples)
                fmt = "—"
                pcol = "—"
            tag = "PASS" if ok else "FAIL"
            results.append((baud, fmt, ok))
            print(f"{baud:>8}  {fmt:>6}  {pcol:>8}  {tag:<6}  {detail}")
            an.reset()
            an.isolate_rs485()

    an.close()

    npass = sum(1 for _, _, ok in results if ok)
    ntot = len(results)
    print()
    print(f"[summary] {npass}/{ntot} passed")
    return 0 if npass == ntot else 2


if __name__ == "__main__":
    sys.exit(main())
