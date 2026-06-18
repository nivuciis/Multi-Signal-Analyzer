#!/usr/bin/env python3
"""
Multi-Signal Analyzer — srpico protocol debugger
=================================================
Connects to the firmware over USB CDC and exercises the sigrok protocol:
  - Identifies the device
  - Configures sample rate, sample count, channels, triggers
  - Runs fixed captures and decodes the RLE stream
  - Displays digital channel states and analog voltages

Usage:
    python3 debug_firmware.py [port] [options]

    python3 debug_firmware.py /dev/ttyACM0
    python3 debug_firmware.py --rate 50000 --samples 512
    python3 debug_firmware.py --analog --show 64
    python3 debug_firmware.py --trigger 2          # rising edge on D0
    python3 debug_firmware.py --raw                # dump raw stream bytes
    python3 debug_firmware.py --test self
    python3 debug_firmware.py --test analog
    python3 debug_firmware.py --test analog-plot      # capture + matplotlib plot
    python3 debug_firmware.py --test trigger

Requires:
    pip install pyserial
    pip install matplotlib   # only for --test analog-plot
"""

import argparse
import re
import sys
import time
import struct

# Done marker: "$<n>+". '$' (0x24) never appears in sample data (data >= 0x80,
# RLE counts 0x30-0x7F), so it uniquely terminates the stream — no trailing null needed.
DONE_MARKER_RE = re.compile(rb'\$(\d+)\+')

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

# ── srpico protocol constants ──────────────────────────────────────────────────
# RLE count bytes (< 0x80) follow a sample and repeat it (libsigrok srpico
# general-mode, process_slice):
#   fine   0x30-0x4F (48-79):  repeats = byte - 47    → 1..32
#   coarse 0x50-0x7F (80-127): repeats = (byte-78)*32 → 64,96,..,1568
# Multiple count bytes accumulate. Data bytes (>= 0x80) are never RLE.
RLE_MIN     = 0x30   # lowest RLE count byte
RLE_MAX     = 0x7F   # highest RLE count byte
DATA_MIN    = 0x80   # sample data bytes always have bit 7 set
ABORT_MARKER = b'!!!'

# Analog: 25700 µV per 7-bit LSB  (3300 mV / 128 levels ≈ 25.78 mV)
ANA_UV_PER_LSB = 25_700


# ── port auto-detection ────────────────────────────────────────────────────────
def find_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        dev = p.device
        if any(k in dev for k in ('ACM', 'usbmodem', 'usbserial', 'ttyUSB')):
            return dev
    ports = list(serial.tools.list_ports.comports())
    return ports[0].device if ports else None


# ── stream decoder ─────────────────────────────────────────────────────────────
def decode_stream(raw: bytes, dig_bps: int, ana_bps: int) -> dict:
    """
    Decode a complete srpico data stream (including done marker).

    raw      : bytes from first data byte up to and including the done marker
    dig_bps  : bytes per digital sample (1 or 2)
    ana_bps  : bytes per analog sample (number of active analog channels)

    Returns dict with keys:
        total_sent   : int   — value from $<n>+ done marker
        digital      : list[int]         — decoded digital channel words
        analog       : list[list[float]] — decoded analog voltages per sample
        errors       : list[str]
    """
    errors = []
    digital = []
    analog  = []
    total_sent = 0

    # ── locate and strip done marker $<n>+ ──
    matches = list(DONE_MARKER_RE.finditer(raw))
    if matches:
        m = matches[-1]
        try:
            total_sent = int(m.group(1))
        except ValueError:
            errors.append("malformed done marker")
        payload = raw[: m.start()]
    else:
        errors.append("no done marker '$<n>+' found — stream may be truncated")
        payload = raw

    # ── strip abort prefix if present ──
    if payload.startswith(ABORT_MARKER):
        errors.append("abort marker '!!!' at stream start")
        payload = payload[len(ABORT_MARKER):]

    # ── decode sample groups ──
    # Frame: [dig_bytes] [ana_ch0..chN] [RLE_count...]
    # A sample (digital bytes >= 0x80, then ana_bps analog bytes) is emitted once;
    # RLE count bytes (< 0x80) that follow repeat the PREVIOUS sample. Multiple
    # count bytes accumulate (libsigrok srpico process_slice convention).
    i = 0
    last_dig = None
    last_ana = None

    while i < len(payload):
        b = payload[i]

        # RLE count byte: repeat the previous sample.
        if b < DATA_MIN:
            if not (RLE_MIN <= b <= RLE_MAX):
                errors.append(f"unexpected byte 0x{b:02X} at offset {i} "
                              f"(not RLE 0x30-0x7F, not data 0x80-0xFF)")
                i += 1
                continue
            if last_dig is None:
                errors.append(f"RLE byte 0x{b:02X} at offset {i} with no prior sample")
                i += 1
                continue
            repeats = (b - 47) if b <= 79 else (b - 78) * 32
            for _ in range(repeats):
                digital.append(last_dig)
                analog.append(list(last_ana))
            i += 1
            continue

        # New sample: digital bytes first.
        if i + dig_bps > len(payload):
            errors.append(f"truncated digital at offset {i}: "
                          f"need {dig_bps} bytes, have {len(payload) - i}")
            break

        dig_bytes = payload[i: i + dig_bps]
        i += dig_bps

        # decode digital: 7 usable bits per byte, little-endian
        dig_val = 0
        for byte_idx, db in enumerate(dig_bytes):
            if not (db & 0x80):
                errors.append(f"digital byte 0x{db:02X} missing high bit")
            dig_val |= (db & 0x7F) << (7 * byte_idx)

        # analog group for this sample
        if ana_bps > 0:
            if i + ana_bps > len(payload):
                errors.append(f"truncated analog at offset {i}: "
                              f"need {ana_bps} bytes, have {len(payload) - i}")
                break
            ab_group = payload[i: i + ana_bps]
            i += ana_bps
            mv_group = []
            for ab in ab_group:
                if not (ab & 0x80):
                    errors.append(f"analog byte 0x{ab:02X} missing high bit")
                mv_group.append(round((ab & 0x7F) * ANA_UV_PER_LSB / 1000.0, 2))
        else:
            mv_group = []

        last_dig = dig_val
        last_ana = mv_group
        digital.append(dig_val)
        analog.append(mv_group)

    return {
        'total_sent': total_sent,
        'digital':    digital,
        'analog':     analog,
        'errors':     errors,
    }


# ── debugger class ─────────────────────────────────────────────────────────────
class FirmwareDebugger:

    def __init__(self, port: str, timeout: float = 5.0):
        self.ser = serial.Serial(port, baudrate=115200, timeout=timeout)
        time.sleep(0.3)
        self.ser.reset_input_buffer()

        self.n_digital     = 12
        self.n_analog      = 3
        self.dig_bps       = 2   # bytes per digital sample
        self._ana_enabled  : list[int] = []

        print(f"[connected] {port}")

    def close(self):
        self.ser.close()

    # ── I/O helpers ───────────────────────────────────────────────────────────

    def _send(self, data: bytes):
        self.ser.write(data)
        self.ser.flush()

    def _read_response(self, timeout: float = 1.0) -> bytes:
        """Read until '*' ACK or timeout."""
        buf = b''
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            c = self.ser.read(1)
            if c:
                buf += c
                if buf.endswith(b'*'):
                    return buf
        return buf

    def _cmd(self, cmd: str, label: str | None = None) -> bool:
        """Send a command, wait for '*' ACK, print result."""
        tag = label or cmd[:12]
        self._send((cmd + '\n').encode())
        resp = self._read_response()
        ok = resp.endswith(b'*')
        marker = 'OK' if ok else f'NO ACK — got {resp!r}'
        print(f"  [{tag}] {marker}")
        return ok

    # ── protocol commands ─────────────────────────────────────────────────────

    def reset(self):
        print("[reset]")
        self._send(b'*')
        time.sleep(0.1)
        self.ser.reset_input_buffer()
        self._ana_enabled = []

    def disable_all_analog(self):
        """Disable all analog channels (firmware restores them to all-on after '*' reset)."""
        for ch in range(self.n_analog):
            self.enable_analog(ch, False)

    def identify(self) -> str:
        self._send(b'i\n')
        # identity response is a plain string, NOT terminated by '*'
        resp = b''
        deadline = time.monotonic() + 1.5
        while time.monotonic() < deadline:
            c = self.ser.read(1)
            if c:
                resp += c
                if resp.endswith(b'*'):
                    break
        ident = resp.rstrip(b'*').strip().decode(errors='replace')
        print(f"[identify] {ident}")
        self._parse_ident(ident)
        return ident

    def _parse_ident(self, ident: str):
        # Format: SRPICO,AxxyDzz,02
        try:
            part = ident.split(',')[1]          # e.g. A031D12
            d_pos = part.index('D')
            a_section = part[1:d_pos]           # '031' → 03 analog, 1 byte each
            self.n_analog  = int(a_section[:2])
            self.n_digital = int(part[d_pos + 1:])
            self.dig_bps   = 1 if self.n_digital <= 7 else (2 if self.n_digital <= 14 else 3)
            print(f"         analog={self.n_analog}  digital={self.n_digital}  "
                  f"dig_bps={self.dig_bps}")
        except Exception as e:
            print(f"  [warn] ident parse failed: {e}")

    def set_sample_rate(self, hz: int) -> bool:
        return self._cmd(f'R{hz}', f'rate={hz}Hz')

    def set_sample_limit(self, n: int) -> bool:
        return self._cmd(f'L{n}', f'limit={n}')

    def enable_digital(self, ch: int, enable: bool = True) -> bool:
        return self._cmd(f'D{1 if enable else 0}{ch}', f'D{ch}={"on" if enable else "off"}')

    def enable_analog(self, ch: int, enable: bool = True) -> bool:
        ok = self._cmd(f'A{1 if enable else 0}{ch}', f'A{ch}={"on" if enable else "off"}')
        if enable and ch not in self._ana_enabled:
            self._ana_enabled.append(ch)
        elif not enable and ch in self._ana_enabled:
            self._ana_enabled.remove(ch)
        return ok

    def analog_scale(self, ch: int) -> str:
        self._send(f'a{ch}\n'.encode())
        resp = self.ser.read(32).strip().decode(errors='replace').rstrip('*')
        print(f"  [scale ch{ch}] {resp}")
        return resp

    def set_pretrigger(self, n: int) -> bool:
        return self._cmd(f'p{n}', f'pretrigger={n}')

    def set_trigger(self, trig_type: int, ch: int) -> bool:
        """
        trig_type : 0=low, 1=high, 2=rise, 3=fall, 4=edge
        ch        : 0-based digital channel index
        """
        idx = ch + 2  # driver adds +2 offset
        return self._cmd(f't{trig_type}{idx}',
                         f'trigger ch={ch} type={trig_type}')

    def clear_trigger(self) -> bool:
        return self._cmd('tn', 'trigger=off')

    # ── capture ───────────────────────────────────────────────────────────────

    def fixed_capture(self, show: int = 32, raw_dump: bool = False) -> dict:
        ana_bps = len(self._ana_enabled)
        print(f"\n[capture] fixed  dig_bps={self.dig_bps}  ana_bps={ana_bps}")

        self._send(b'F\n')
        raw = self._read_stream(timeout=10.0)

        if raw_dump:
            self._print_raw(raw)

        result = decode_stream(raw, self.dig_bps, ana_bps)
        self._print_result(result, show)
        return result

    def _read_stream(self, timeout: float = 10.0) -> bytes:
        """Read until null stop-byte or timeout."""
        buf = bytearray()
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            chunk = self.ser.read(512)
            if chunk:
                buf.extend(chunk)
                # done when we see the "$<digits>+" marker ('$' can't be sample data)
                m = DONE_MARKER_RE.search(bytes(buf))
                if m:
                    return bytes(buf[: m.end()])
                if ABORT_MARKER in buf:
                    return bytes(buf)

        print("  [warn] stream read timed out")
        return bytes(buf)

    # ── display helpers ───────────────────────────────────────────────────────

    def _print_result(self, result: dict, show: int):
        n_dig = len(result['digital'])
        n_ana = len(result['analog'])
        print(f"\n  done-marker total : {result['total_sent']} bytes")
        print(f"  digital samples  : {n_dig}")
        print(f"  analog samples   : {n_ana}")

        for e in result['errors']:
            print(f"  [ERROR] {e}")

        if not result['digital']:
            print("  (no samples decoded)")
            return

        n_show = min(show, n_dig)
        n_ch   = self.n_digital
        print(f"\n  First {n_show} samples:")
        print(f"  {'idx':>5}  {'digital (channels)':{'<'}{n_ch + 4}}  analog (mV)")
        print(f"  {'-'*5}  {'-'*(n_ch + 4)}  {'-'*30}")

        for i in range(n_show):
            d  = result['digital'][i]
            ch = ''.join(str((d >> bit) & 1) for bit in range(n_ch - 1, -1, -1))
            av = result['analog'][i]
            av_str = '  '.join(f"{v:7.1f}" for v in av) if av else '—'
            print(f"  {i:5d}  {ch}  {av_str}")

        if n_dig > n_show:
            print(f"  … ({n_dig - n_show} more samples)")

    @staticmethod
    def _print_raw(raw: bytes):
        print(f"\n  raw stream ({len(raw)} bytes):")
        for i in range(0, len(raw), 16):
            chunk = raw[i: i + 16]
            hex_part = ' '.join(f'{b:02X}' for b in chunk)
            asc_part = ''.join(chr(b) if 0x20 <= b < 0x7F else '.' for b in chunk)
            print(f"  {i:04X}  {hex_part:<48}  {asc_part}")

    # ── predefined test sequences ─────────────────────────────────────────────

    def test_self(self) -> bool:
        """Identify + digital-only fixed capture."""
        print("\n══ self test ══")
        self.reset()
        ident = self.identify()
        if 'SRPICO' not in ident:
            print("[FAIL] identify")
            return False

        self.disable_all_analog()
        self.set_sample_rate(10_000)
        self.set_sample_limit(128)
        for ch in range(self.n_digital):
            self.enable_digital(ch)

        result = self.fixed_capture(show=16)
        ok = len(result['digital']) == 128 and not result['errors']
        print(f"\n[self test] {'PASS' if ok else 'FAIL'}")
        return ok

    def test_analog(self):
        """Digital + all analog channels."""
        print("\n══ analog test ══")
        self.reset()
        self.identify()
        self.disable_all_analog()
        self.set_sample_rate(5_000)
        self.set_sample_limit(64)

        for ch in range(self.n_digital):
            self.enable_digital(ch)
        for ch in range(self.n_analog):
            self.enable_analog(ch)
        for ch in range(self.n_analog):
            self.analog_scale(ch)

        self.fixed_capture(show=16)

    def test_analog_plot(self, rate: int = 5_000, samples: int = 512,
                         save_path: str | None = None):
        """Capture analog channels and plot the voltage traces with matplotlib.

        Each enabled analog channel is drawn as its own line vs time (seconds).
        Pass save_path to write a PNG instead of opening an interactive window.
        """
        try:
            import matplotlib
            if save_path:
                matplotlib.use('Agg')
            import matplotlib.pyplot as plt
        except ImportError:
            print("Error: matplotlib not installed. Run: pip install matplotlib")
            return None

        print(f"\n══ analog plot test  rate={rate}Hz  samples={samples} ══")
        self.reset()
        self.identify()
        self.disable_all_analog()
        self.set_sample_rate(rate)
        self.set_sample_limit(samples)

        # disable digital channels — analog-only capture keeps the plot focused
        for ch in range(self.n_digital):
            self.enable_digital(ch, False)
        for ch in range(self.n_analog):
            self.enable_analog(ch, True)
        for ch in range(self.n_analog):
            self.analog_scale(ch)

        result = self.fixed_capture(show=8)
        if not result['analog'] or not result['analog'][0]:
            print("[FAIL] no analog samples decoded")
            return result

        n_ch = len(result['analog'][0])
        n    = len(result['analog'])
        dt   = 1.0 / rate
        t    = [i * dt for i in range(n)]
        # transpose: series[ch] = [v0, v1, ...]
        series = [[row[ch] if ch < len(row) else 0.0
                   for row in result['analog']] for ch in range(n_ch)]

        fig, ax = plt.subplots(figsize=(10, 4 + n_ch * 0.4))
        for ch, vals in enumerate(series):
            ax.plot(t, vals, label=f'A{ch}', linewidth=1.0)
        ax.set_xlabel('time (s)')
        ax.set_ylabel('voltage (mV)')
        ax.set_title(f'Analog capture — {rate} Hz, {n} samples')
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best')
        fig.tight_layout()

        if save_path:
            fig.savefig(save_path, dpi=120)
            print(f"[plot] saved to {save_path}")
        else:
            print("[plot] showing window (close to continue)")
            plt.show()

        return result

    def test_trigger(self, ch: int = 0, trig_type: int = 2):
        """Triggered capture (default: rising edge on D0)."""
        names = {0: 'low', 1: 'high', 2: 'rise', 3: 'fall', 4: 'edge'}
        print(f"\n══ trigger test — D{ch} {names.get(trig_type, '?')} ══")
        self.reset()
        self.identify()
        self.disable_all_analog()
        self.set_sample_rate(50_000)
        self.set_sample_limit(256)

        for c in range(self.n_digital):
            self.enable_digital(c)

        self.set_pretrigger(0)
        self.set_trigger(trig_type, ch)

        print(f"  Waiting for trigger on D{ch} …")
        self.fixed_capture(show=32)


# ── CLI ────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='srpico firmware debugger',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)

    parser.add_argument('port', nargs='?',
                        help='Serial port (auto-detected if omitted)')
    parser.add_argument('--rate', type=int, default=10_000,
                        metavar='HZ',
                        help='Sample rate in Hz (default 10000)')
    parser.add_argument('--samples', type=int, default=256,
                        metavar='N',
                        help='Number of samples (default 256)')
    parser.add_argument('--analog', action='store_true',
                        help='Enable all analog channels')
    parser.add_argument('--trigger', type=int, default=None,
                        choices=[0, 1, 2, 3, 4],
                        metavar='TYPE',
                        help='Trigger type on D0: 0=low 1=high 2=rise 3=fall 4=edge')
    parser.add_argument('--show', type=int, default=32,
                        metavar='N',
                        help='Number of samples to display (default 32)')
    parser.add_argument('--raw', action='store_true',
                        help='Dump raw stream bytes')
    parser.add_argument('--test',
                        choices=['self', 'analog', 'analog-plot', 'trigger'],
                        help='Run a predefined test sequence')
    parser.add_argument('--trig-ch', type=int, default=0,
                        metavar='CH',
                        help='Trigger channel for --test trigger (default 0)')
    parser.add_argument('--plot-save', metavar='PATH',
                        help='For --test analog-plot: save PNG instead of showing window')

    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        print("Error: no serial port found. Pass the port as an argument, e.g.:")
        print("  python3 debug_firmware.py /dev/ttyACM0")
        sys.exit(1)

    dbg = FirmwareDebugger(port)
    try:
        if args.test == 'self':
            dbg.test_self()
        elif args.test == 'analog':
            dbg.test_analog()
        elif args.test == 'analog-plot':
            dbg.test_analog_plot(rate=args.rate, samples=args.samples,
                                 save_path=args.plot_save)
        elif args.test == 'trigger':
            dbg.test_trigger(ch=args.trig_ch, trig_type=args.trigger or 2)
        else:
            dbg.reset()
            dbg.identify()
            dbg.disable_all_analog()
            dbg.set_sample_rate(args.rate)
            dbg.set_sample_limit(args.samples)

            for ch in range(dbg.n_digital):
                dbg.enable_digital(ch)

            if args.analog:
                for ch in range(dbg.n_analog):
                    dbg.enable_analog(ch)

            if args.trigger is not None:
                dbg.set_pretrigger(0)
                dbg.set_trigger(args.trigger, 0)

            dbg.fixed_capture(show=args.show, raw_dump=args.raw)

    except KeyboardInterrupt:
        print("\n[interrupted]")
    finally:
        dbg.close()


if __name__ == '__main__':
    main()
