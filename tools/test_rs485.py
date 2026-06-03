#!/usr/bin/env python3
"""
test_rs485.py — continuously send a string over a USB serial port.

Sends the given string to the serial port in a loop, so you can watch the
traffic on the RS485 bus / logic analyzer.

Baudrate vs sample rate
    PulseView's UART decoder needs several samples per bit to decode. Rule of
    thumb: capture samplerate >= 5x baud (10x recommended). Below that the line
    is undersampled and the decoder shows "Break condition" / garbage bytes.
    This script prints the samplerate to set in PulseView for the chosen baud.
    To run at the firmware minimum (5 kHz), use --baud 500.

Usage:
    python3 tools/test_rs485.py "hello world"
    python3 tools/test_rs485.py "hello" --port /dev/ttyUSB0 --baud 9600 --interval 0.5
    python3 tools/test_rs485.py "ping" --no-newline --count 10
    python3 tools/test_rs485.py "slow" --baud 500   # decodes at PulseView 5 kHz

Requirements:
    pip install pyserial
"""

import argparse
import sys
import time

# PulseView UART decoder needs several samples per bit. 5x is the bare minimum,
# 10x is comfortable. See develop.md "RS485 — baudrate vs taxa de aquisição".
MIN_OVERSAMPLE = 5
RECOMMENDED_OVERSAMPLE = 10

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial", file=sys.stderr)
    sys.exit(1)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Continuously send a string over a USB serial port.")
    p.add_argument("text",
                   help="String to send repeatedly over the port")
    p.add_argument("--port", default="/dev/ttyUSB0",
                   help="Serial port to write to (default: /dev/ttyUSB0)")
    p.add_argument("--baud", type=int, default=9600,
                   help="Baud rate (default: 9600)")
    p.add_argument("--interval", type=float, default=1.0,
                   help="Seconds between sends (default: 1.0)")
    p.add_argument("--count", type=int, default=0,
                   help="Number of sends before stopping (0 = infinite)")
    p.add_argument("--no-newline", action="store_true",
                   help="Do not append a newline to the string")
    p.add_argument("--oversample", type=int, default=RECOMMENDED_OVERSAMPLE,
                   help=f"Samples-per-bit factor for the recommended PulseView "
                        f"samplerate (default: {RECOMMENDED_OVERSAMPLE})")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    payload = args.text if args.no_newline else args.text + "\n"
    data = payload.encode()

    try:
        port = serial.Serial(args.port, baudrate=args.baud, timeout=1)
    except serial.SerialException as exc:
        print(f"ERROR opening {args.port}: {exc}", file=sys.stderr)
        return 1

    recommended = args.baud * args.oversample
    minimum = args.baud * MIN_OVERSAMPLE
    print(f"PulseView: set samplerate >= {recommended} Hz ({args.oversample}x baud) "
          f"to decode. Bare minimum: {minimum} Hz ({MIN_OVERSAMPLE}x). "
          f"UART decoder: {args.baud} baud, 8/N/1, LSB-first.")

    print(f"Sending {data!r} to {args.port} @ {args.baud} baud "
          f"every {args.interval}s "
          f"({'infinite' if args.count == 0 else args.count} times). "
          f"Ctrl-C to stop.")

    sent = 0
    try:
        while args.count == 0 or sent < args.count:
            port.write(data)
            port.flush()
            sent += 1
            print(f"  [{sent}] sent {len(data)} bytes... data: {data.hex()}")
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\n[interrupted]")
    finally:
        port.close()

    print(f"Done. {sent} messages sent.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
