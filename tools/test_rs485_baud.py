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
# RLE count bytes (< 0x80) follow a sample and repeat it (libsigrok srpico
# process_slice): fine 0x30-0x4F → byte-47 (1..32); coarse 0x50-0x7F →
# (byte-78)*32 (64..1568). Counts accumulate. Data bytes (>= 0x80) are never RLE.
RLE_MIN = 0x30
RLE_MAX = 0x7F
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
    last_bit = None
    while i < n:
        b = payload[i]
        # RLE count byte: repeat the previous sample's bit.
        if b < DATA_MIN:
            if RLE_MIN <= b <= RLE_MAX and last_bit is not None:
                repeats = (b - 47) if b <= 79 else (b - 78) * 32
                bits.extend([last_bit] * repeats)
            i += 1
            continue
        if i + dig_bps > n:
            break
        dig_val = 0
        for k in range(dig_bps):
            dig_val |= (payload[i + k] & 0x7F) << (7 * k)
        i += dig_bps
        last_bit = (dig_val >> RS485_CHANNEL) & 1
        bits.append(last_bit)
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


# ── interactive transmitter (live-reconfigurable) ────────────────────────────────
class InteractiveTx:
    """Continuously blast a payload on the source port; reconfigure on the fly.

    Use when PulseView owns the analyzer and you want to manually tune the UART
    decoder while changing the wire format live. This object owns only the
    transmitter — it never touches the analyzer port.
    """

    def __init__(self, port, baud, databits, parity, stopbits, payload, gap):
        self.port = port
        self.baud = baud
        self.databits = databits
        self.parity = parity
        self.stopbits = stopbits
        self.payload = payload
        self.gap = gap
        self.sent = 0  # frames (payload writes) emitted so far — live TX counter
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._reopen = threading.Event()
        self.error = None
        self._thr = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self._thr.start()

    def stop(self):
        self._stop.set()
        self._thr.join(timeout=2)

    def update(self, **kw):
        """Set any of {baud, databits, parity, stopbits, payload, gap} and reopen."""
        with self._lock:
            for k, v in kw.items():
                setattr(self, k, v)
            self.sent = 0  # restart the counter so the new format's stream is obvious
        self._reopen.set()

    def _open(self):
        with self._lock:
            cfg = dict(
                port=self.port,
                baudrate=self.baud,
                bytesize=BYTESIZE_MAP[self.databits],
                parity=PARITY_MAP[self.parity],
                stopbits=STOPBITS_MAP[self.stopbits],
                timeout=1,
            )
        return serial.Serial(**cfg)

    def _run(self):
        s = None
        try:
            s = self._open()
            while not self._stop.is_set():
                if self._reopen.is_set():
                    self._reopen.clear()
                    s.close()
                    s = self._open()
                with self._lock:
                    pat, gap = self.payload, self.gap
                s.write(pat)
                s.flush()
                self.sent += 1
                if gap > 0:
                    time.sleep(gap)
        except serial.SerialException as e:
            self.error = str(e)
        finally:
            if s is not None:
                try:
                    s.close()
                except serial.SerialException:
                    pass


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


INTERACTIVE_HELP = """\
commands (change format live; transmitter restarts on each change):
  baud <n>            set baud rate            e.g. baud 115200
  data <5..8>         set data bits            e.g. data 7
  parity <N|E|O|M|S>  set parity               e.g. parity E
  stop <1|2>          set stop bits            e.g. stop 2
  gap <seconds>       idle gap between frames  e.g. gap 0.1   (0 = back-to-back)
  payload <text>      send literal text        e.g. payload Hello World
  payload :hello      send preset "Hello World"
  payload :1k         send 1k deterministic byte ramp
  show                print current config
  help / ?            this help
  quit / q            stop and exit
"""


def _fmt_payload(payload, name):
    return name if name else f"{len(payload)} B literal"


def run_interactive(args) -> int:
    if not args.source:
        print("Error: interactive mode needs --source (transmitter port).", file=sys.stderr)
        return 1

    baud = args.bauds[0]
    databits = args.databits[0]
    parity = args.parity[0]
    stopbits = args.stopbits[0]
    pname = args.payloads[0]
    payload = PAYLOADS[pname]

    tx = InteractiveTx(args.source, baud, databits, parity, stopbits, payload, args.gap)
    tx.start()

    def status():
        if tx.error:
            print(f"[tx error] {tx.error}")
            return
        print(f"[tx] {args.source}  baud={tx.baud}  fmt={tx.databits}{tx.parity}{tx.stopbits}"
              f"  gap={tx.gap}s  payload={_fmt_payload(tx.payload, pname)}")

    print(f"[interactive] transmitting on {args.source} — PulseView owns the analyzer.")
    print(INTERACTIVE_HELP)
    status()

    try:
        while True:
            try:
                line = input("rs485> ").strip()
            except EOFError:
                break
            if not line:
                if tx.error:
                    status()
                continue
            parts = line.split(maxsplit=1)
            cmd = parts[0].lower()
            arg = parts[1] if len(parts) > 1 else ""

            try:
                if cmd in ("quit", "q", "exit"):
                    break
                elif cmd in ("help", "?", "h"):
                    print(INTERACTIVE_HELP)
                elif cmd in ("show", "status", "s"):
                    pass  # falls through to status() below
                elif cmd == "baud":
                    tx.update(baud=int(arg))
                elif cmd in ("data", "databits"):
                    n = int(arg)
                    if n not in BYTESIZE_MAP:
                        raise ValueError("data bits must be 5..8")
                    tx.update(databits=n)
                elif cmd == "parity":
                    p = arg.upper()
                    if p not in PARITY_MAP:
                        raise ValueError("parity must be N|E|O|M|S")
                    tx.update(parity=p)
                elif cmd == "stop":
                    n = int(arg)
                    if n not in STOPBITS_MAP:
                        raise ValueError("stop bits must be 1 or 2")
                    tx.update(stopbits=n)
                elif cmd == "gap":
                    tx.update(gap=max(0.0, float(arg)))
                elif cmd == "payload":
                    if arg.startswith(":"):
                        key = arg[1:]
                        if key not in PAYLOADS:
                            raise ValueError(f"preset must be one of {list(PAYLOADS)}")
                        pname = key
                        tx.update(payload=PAYLOADS[key])
                    elif arg:
                        pname = ""
                        tx.update(payload=arg.encode())
                    else:
                        raise ValueError("payload needs text or :hello / :1k")
                else:
                    print(f"unknown command: {cmd!r} (try 'help')")
                    continue
            except ValueError as e:
                print(f"bad arg: {e}")
                continue

            status()
    except KeyboardInterrupt:
        pass
    finally:
        tx.stop()
        print("\n[interactive] stopped.")
    return 0


# ── single-key live menu (raw terminal) ──────────────────────────────────────────
# Digit keys map straight to a baud preset. Edit freely; key '0' = 9100 per request.
BAUD_KEYS = {
    "1": 9600,
    "2": 19200,
    "3": 38400,
    "4": 57600,
    "5": 115200,
    "6": 230400,
    "7": 460800,
    "8": 921600,
    "9": 1_000_000,
    "0": 9100,
}
PARITY_CYCLE = ["N", "E", "O", "M", "S"]
DATA_CYCLE = [5, 6, 7, 8]
GAP_STEP = 0.05

# ── ANSI styling ──────────────────────────────────────────────────────────────
RST = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
CYAN = "\033[36m"
GREY = "\033[90m"
WHITE = "\033[97m"
ACTIVE = "\033[1;30;42m"  # bold black on green — selected value
KEY = "\033[1;33m"        # bold yellow — hotkeys
TITLE = "\033[1;36m"      # bold cyan
ERRC = "\033[1;37;41m"    # bold white on red

_ANSI_RE = re.compile(r"\033\[[0-9;]*m")
BOX_W = 62      # inner width; recomputed per-render from the live terminal size
BOX_W_MIN = 48
BOX_W_MAX = 100


def _term_box_w() -> int:
    import shutil
    cols = shutil.get_terminal_size(fallback=(80, 24)).columns
    return max(BOX_W_MIN, min(BOX_W_MAX, cols - 4))


def _vlen(s: str) -> int:
    return len(_ANSI_RE.sub("", s))


def _row(s: str = "") -> str:
    pad = BOX_W - _vlen(s)
    return f"{GREY}│{RST} {s}{' ' * max(0, pad)} {GREY}│{RST}"


def _rule(left="├", right="┤") -> str:
    return f"{GREY}{left}{'─' * (BOX_W + 2)}{right}{RST}"


def _key(k: str) -> str:
    return f"{KEY}{k}{RST}"


def _preset(k: str, val, active: bool) -> str:
    label = f" {k}:{val} "
    return f"{ACTIVE}{label}{RST}" if active else f"{GREY}{k}:{RST}{WHITE}{val}{RST} "


def build_menu(tx, state) -> str:
    global BOX_W
    BOX_W = _term_box_w()  # responsive: track current terminal width
    L = []
    L.append(f"{GREY}┌{'─' * (BOX_W + 2)}┐{RST}")
    L.append(_row(f"{TITLE}RS485 live transmitter{RST}  {DIM}PulseView owns the analyzer{RST}"))
    L.append(_rule())

    # baud presets, two rows, active highlighted
    items = list(BAUD_KEYS.items())
    half = (len(items) + 1) // 2
    for chunk in (items[:half], items[half:]):
        cells = "".join(_preset(k, v, tx.baud == v) for k, v in chunk)
        L.append(_row(f"{CYAN}baud {RST} {cells}"))
    L.append(_rule())

    # format row: parity / data / stop, each value highlighted
    par = "".join(
        (f"{ACTIVE} {p} {RST}" if p == tx.parity else f"{GREY}{p}{RST}") + " "
        for p in PARITY_CYCLE
    )
    dat = "".join(
        (f"{ACTIVE} {d} {RST}" if d == tx.databits else f"{GREY}{d}{RST}") + " "
        for d in DATA_CYCLE
    )
    stp = "".join(
        (f"{ACTIVE} {s} {RST}" if s == tx.stopbits else f"{GREY}{s}{RST}") + " "
        for s in (1, 2)
    )
    L.append(_row(f"{_key('p')} parity  {par}"))
    L.append(_row(f"{_key('d')} data    {dat}"))
    L.append(_row(f"{_key('s')} stop    {stp}"))
    L.append(_rule())

    # gap + payload
    pl = state["pname"] if state["pname"] else f"{len(tx.payload)} B literal"
    L.append(_row(f"{_key('+')}/{_key('-')} gap   {WHITE}{tx.gap:.2f}s{RST}"
                  f"   {GREY}(0 = back-to-back){RST}"))
    L.append(_row(f"{_key('h')} hello   {_key('k')} 1k ramp"
                  f"     {CYAN}payload{RST} {WHITE}{pl}{RST}"))
    L.append(_rule())

    # live status line — spinner + frame counter prove it's transmitting now
    fmt = f"{tx.databits}{tx.parity}{tx.stopbits}"
    if tx.error:
        L.append(_row(f"{ERRC} TX ERROR {RST} {tx.error}"))
    else:
        spin = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"[state.get("tick", 0) % 10]
        L.append(_row(f"\033[1;32m{spin} TX{RST}  {BOLD}{WHITE}{tx.baud}{RST} baud  "
                      f"{BOLD}{WHITE}{fmt}{RST}  gap {tx.gap:.2f}s  "
                      f"{GREY}sent{RST} {WHITE}{tx.sent}{RST} frames"))
    L.append(f"{GREY}└{'─' * (BOX_W + 2)}┘{RST}")
    L.append("")
    L.append(f"{_key('r')} redraw   {_key('q')} quit")
    if state["msg"]:
        L.append(f"{DIM}{state['msg']}{RST}")
    return "\n".join(L)


def run_menu(args) -> int:
    if not args.source:
        print("Error: menu mode needs --source (transmitter port).", file=sys.stderr)
        return 1
    if not sys.stdin.isatty():
        print("Error: menu mode needs an interactive terminal (stdin not a tty).",
              file=sys.stderr)
        return 1

    import termios
    import tty

    baud = args.bauds[0]
    databits = args.databits[0]
    parity = args.parity[0]
    stopbits = args.stopbits[0]
    pname = args.payloads[0]
    payload = PAYLOADS[pname]

    tx = InteractiveTx(args.source, baud, databits, parity, stopbits, payload, args.gap)
    tx.start()

    import select

    state = {"pname": pname, "msg": "", "tick": 0}

    def render():
        # Flicker-free: home the cursor and clear each line in place (no full wipe),
        # so the spinner/counter can refresh continuously without the screen blinking.
        out = ["\033[H"]
        for ln in build_menu(tx, state).split("\n"):
            out.append(ln + "\033[K")
        out.append("\n key> \033[K\033[J")
        sys.stdout.write("\n".join(out))
        sys.stdout.flush()

    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        sys.stdout.write("\033[2J")  # one full clear up front
        render()
        while True:
            # Wait up to 150 ms for a key; on timeout just refresh the live status.
            ready, _, _ = select.select([sys.stdin], [], [], 0.15)
            if not ready:
                state["tick"] += 1
                render()
                continue
            ch = sys.stdin.read(1)
            if not ch:
                continue
            state["msg"] = ""
            if ch in ("q", "\x03", "\x04"):  # q, Ctrl-C, Ctrl-D
                break
            elif ch in BAUD_KEYS:
                tx.update(baud=BAUD_KEYS[ch])
            elif ch == "p":
                nxt = PARITY_CYCLE[(PARITY_CYCLE.index(tx.parity) + 1) % len(PARITY_CYCLE)]
                tx.update(parity=nxt)
            elif ch == "d":
                nxt = DATA_CYCLE[(DATA_CYCLE.index(tx.databits) + 1) % len(DATA_CYCLE)]
                tx.update(databits=nxt)
            elif ch == "s":
                tx.update(stopbits=1 if tx.stopbits == 2 else 2)
            elif ch in ("+", "="):
                tx.update(gap=round(tx.gap + GAP_STEP, 3))
            elif ch in ("-", "_"):
                tx.update(gap=round(max(0.0, tx.gap - GAP_STEP), 3))
            elif ch == "h":
                state["pname"] = "hello"
                tx.update(payload=PAYLOADS["hello"])
            elif ch == "k":
                state["pname"] = "1k"
                tx.update(payload=PAYLOADS["1k"])
            elif ch == "r":
                pass  # just redraw
            else:
                state["msg"] = f"unbound key: {ch!r}"
            render()
    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        tx.stop()
        sys.stdout.write("\n[menu] stopped.\n")
        sys.stdout.flush()
    return 0


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
    ap.add_argument("-i", "--interactive", action="store_true",
                    help="Live transmitter (typed commands): change baud/data/parity/stop on the "
                         "fly while you tune the decoder in PulseView. Needs --source.")
    ap.add_argument("-m", "--menu", action="store_true",
                    help="Live transmitter with an on-screen single-key menu (one keystroke = one "
                         "change, e.g. '0' -> baud 9100). Needs --source; ignores the analyzer.")
    ap.add_argument("--gap", type=float, default=0.05,
                    help="Interactive idle gap (s) between frames (default 0.05, 0 = back-to-back)")
    args = ap.parse_args()

    # First value of each matrix list seeds the live config.
    if args.menu:
        return run_menu(args)
    if args.interactive:
        return run_interactive(args)

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
