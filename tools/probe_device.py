#!/usr/bin/env python3
"""Quick raw-stream probe for the analyzer — digital-only fixed capture.

Mirrors what PulseView sends for an all-digital / no-analog acquisition, then
dumps exactly what the device streams back so we can see whether the bytes are
valid sample data, a device abort ('!'), or nothing. Close PulseView first
(the CDC port is single-open).

  python3 tools/probe_device.py [--port /dev/ttyACM1] [--rate 1000000] [--limit 4096]
"""
import argparse
import time
import sys

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM1")
    ap.add_argument("--rate", type=int, default=1_000_000)
    ap.add_argument("--limit", type=int, default=4096)
    ap.add_argument("--digital", type=int, default=13, help="number of digital channels to enable")
    ap.add_argument("--analog", type=int, default=3, help="number of analog channels present")
    ap.add_argument("--mode", choices=["F", "C"], default="F",
                    help="F=fixed (default), C=continuous (streams until '+', like PulseView SW-trigger)")
    ap.add_argument("--secs", type=float, default=3.0, help="continuous read window before '+'")
    args = ap.parse_args()

    s = serial.Serial(args.port, 115200, timeout=0.05)

    def read_until_star(deadline_s=1.0):
        buf = bytearray()
        end = time.monotonic() + deadline_s
        while time.monotonic() < end:
            c = s.read(1)
            if c:
                buf += c
                if c == b"*":
                    break
        return bytes(buf)

    def ack(b):
        s.write(b + b"\n"); s.flush()
        return read_until_star()

    # reset + identify
    s.write(b"*"); s.flush(); time.sleep(0.2); s.reset_input_buffer()
    s.write(b"i\n"); s.flush()
    print("IDENT :", read_until_star(1.5))

    # disable all analog, enable the digital channels — pure digital capture
    for ch in range(args.analog):
        print(f"A0{ch:02d} :", ack(f"A0{ch:02d}".encode()))
    for ch in range(args.digital):
        print(f"D1{ch:02d} :", ack(f"D1{ch:02d}".encode()))

    print("R     :", ack(f"R{args.rate}".encode()))
    print("L     :", ack(f"L{args.limit}".encode()))

    # capture
    s.reset_input_buffer()
    if args.mode == "C":
        print(f"\n[continuous] streaming {args.secs}s then sending '+'")
        s.write(b"C\n"); s.flush()
        raw = bytearray()
        deadline = time.monotonic() + args.secs
        while time.monotonic() < deadline:
            chunk = s.read(8192)
            if chunk:
                raw.extend(chunk)
                if b"!!!" in raw:
                    break
        s.write(b"+"); s.flush()
        time.sleep(0.2)
        raw.extend(s.read(65536))  # drain whatever was in flight
    else:
        s.write(b"F\n"); s.flush()
        raw = bytearray()
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline:
            chunk = s.read(8192)
            if chunk:
                raw.extend(chunk)
                if b"$" in chunk and b"+" in raw[-8:]:
                    break
                if b"!!!" in raw:
                    break
        s.write(b"+"); s.flush()
    s.close()

    n = len(raw)
    data = sum(1 for b in raw if b >= 0x80)
    rle = sum(1 for b in raw if 0x30 <= b <= 0x7F)
    print(f"\nTOTAL bytes received : {n}")
    print(f"  data bytes (>=0x80): {data}")
    print(f"  RLE bytes (0x30-7F): {rle}")
    print(f"  contains '!!!'     : {b'!!!' in raw}")
    print(f"  first 48 bytes hex : {bytes(raw[:48]).hex()}")
    print(f"  last  16 bytes     : {bytes(raw[-16:])!r}")


if __name__ == "__main__":
    sys.exit(main())
