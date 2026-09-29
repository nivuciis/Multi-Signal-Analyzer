#!/usr/bin/env python3
"""benchmarks/m3_continuous_loss.py — M3: Captura contínua e perda de amostras
(TCC Cap.3, Sec. "Métricas e procedimentos experimentais").

Continuous ('C') capture of the counter pattern with simultaneous host
transmission for a fixed window (default 60s); measures the effective
delivered rate, the number/size of gaps (breaks in the counter sequence),
and reports the requested rate for the amostragem x USB conflict analysis
(hypothesis item iii).

Note: firmware-reported overflow counters are not currently exposed over
the wire (only the abort marker "!!!", which this script also records) —
gap detection from the decoded stream is the primary signal here.

Usage:
  python3 benchmarks/m3_continuous_loss.py --firmware-label A --port /dev/ttyACM0
  python3 benchmarks/m3_continuous_loss.py --firmware-label B --duration 60 --reps 30
"""
import argparse
import sys
import time

from common import (
    CHANNEL_SWEEP,
    N_REPETITIONS,
    RESULTS_DIR,
    connect,
    csv_writer,
    decode_continuous_stream,
    digital_bytes_per_sample,
    find_counter_gaps,
    log_rate_sweep,
    setup_channels,
)

# SIGROK_SAMPLE_LIMIT_MAX (includes/handles/handles_internal.h): the largest
# chunk size the firmware allows per 'L', hence per continuous-mode
# re-arm cycle. Using the max here minimizes (but, per the protocol's own
# cap, cannot eliminate) how often a "$<n>+" marker is re-embedded
# mid-stream during the capture window.
SIGROK_SAMPLE_LIMIT_MAX = 1_000_000


def continuous_capture(dbg, duration_s):
    """Run 'C' for duration_s, then stop it and drain what's left.

    @return (raw_bytes, aborted_by_device: bool)
    """
    dbg.ser.reset_input_buffer()
    dbg._send(b"C\n")
    raw = bytearray()
    deadline = time.monotonic() + duration_s
    aborted = False
    while time.monotonic() < deadline:
        chunk = dbg.ser.read(4096)
        if chunk:
            raw.extend(chunk)
            if b"!!!" in raw[-8:]:
                aborted = True
                break
    dbg._send(b"+")
    time.sleep(0.3)
    raw.extend(dbg.ser.read(1 << 20))
    return bytes(raw), aborted


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=None)
    ap.add_argument("--firmware-label", required=True)
    ap.add_argument("--duration", type=float, default=60.0, help="capture window in seconds")
    ap.add_argument("--reps", type=int, default=N_REPETITIONS)
    ap.add_argument("--channels", default=",".join(str(c) for c in CHANNEL_SWEEP))
    ap.add_argument("--rate-min", type=int, default=None)
    ap.add_argument("--rate-max", type=int, default=None)
    ap.add_argument("--points-per-decade", type=int, default=4,
                    help="M3 is slow (one full --duration per point) — coarser sweep than M1 by default")
    ap.add_argument("--output", default=None)
    args = ap.parse_args()

    channels = [int(c) for c in args.channels.split(",")]
    rate_kwargs = {}
    if args.rate_min:
        rate_kwargs["rate_min"] = args.rate_min
    if args.rate_max:
        rate_kwargs["rate_max"] = args.rate_max
    rates = log_rate_sweep(points_per_decade=args.points_per_decade, **rate_kwargs)

    out_path = args.output or (RESULTS_DIR / f"m3_{args.firmware_label}.csv")
    f, writer = csv_writer(
        out_path,
        ["firmware", "channels", "rate_hz", "rep", "requested_samples", "delivered_samples",
         "effective_rate_hz", "n_gaps", "gap_samples_total", "aborted", "decode_errors"],
    )

    dbg = connect(args.port)
    try:
        for n_channels in channels:
            print(f"\n== M3: {n_channels} channel(s) ==")
            dig_bps = digital_bytes_per_sample(n_channels)
            for rate_hz in rates:
                for rep in range(args.reps):
                    setup_channels(dbg, n_channels, rate_hz, n_samples=SIGROK_SAMPLE_LIMIT_MAX)
                    t0 = time.monotonic()
                    raw, aborted = continuous_capture(dbg, args.duration)
                    elapsed = time.monotonic() - t0

                    result = decode_continuous_stream(raw, dig_bps)
                    delivered = len(result["digital"])
                    gaps = find_counter_gaps(result["digital"], n_channels)
                    gap_total = sum(g["size"] for g in gaps)
                    requested = int(rate_hz * args.duration)
                    effective_rate = delivered / elapsed if elapsed > 0 else 0.0

                    writer.writerow({
                        "firmware": args.firmware_label,
                        "channels": n_channels,
                        "rate_hz": rate_hz,
                        "rep": rep,
                        "requested_samples": requested,
                        "delivered_samples": delivered,
                        "effective_rate_hz": round(effective_rate, 2),
                        "n_gaps": len(gaps),
                        "gap_samples_total": gap_total,
                        "aborted": int(aborted),
                        "decode_errors": len(result["errors"]),
                    })
                    f.flush()
                    print(f"  rate={rate_hz:>10} Hz rep={rep:>2}  "
                          f"delivered={delivered:>8}  gaps={len(gaps):>3}"
                          f"  eff_rate={effective_rate:>10.1f} Hz"
                          f"{'  ABORTED' if aborted else ''}")
    finally:
        dbg.close()
        f.close()

    print(f"\nResults written to {out_path}")


if __name__ == "__main__":
    sys.exit(main())
