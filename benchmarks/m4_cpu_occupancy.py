#!/usr/bin/env python3
"""benchmarks/m4_cpu_occupancy.py — M4: Ocupação de CPU (TCC Cap.3, Sec.
"Métricas e procedimentos experimentais").

Runs a fixed capture, then queries the 'o' command (busy/total DWT cycles
for that capture — see src/occupancy.c and handle_get_cpu_occupancy.c) to
compute the occupancy fraction. Requires firmware built with the DWT
occupancy instrumentation; plain sigrok-pico firmware without it will make
this script raise (see common.query_occupancy).

What counts as "busy" differs by firmware and is defined in
src/sigrok_handler.c, not by this script:
  - Firmware A: only the RLE+TX regions around each
    ana_send_packet_channels() call (DMA-wait polling is NOT busy).
  - Firmware B: the whole capture (sampling is inherently CPU-bound).

Usage:
  python3 benchmarks/m4_cpu_occupancy.py --firmware-label A --port /dev/ttyACM0
  python3 benchmarks/m4_cpu_occupancy.py --firmware-label B --samples 65536 --reps 30
"""
import argparse
import sys

from common import (
    CHANNEL_SWEEP,
    N_REPETITIONS,
    RESULTS_DIR,
    capture_fixed_digital,
    connect,
    csv_writer,
    log_rate_sweep,
    query_occupancy,
    setup_channels,
)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=None)
    ap.add_argument("--firmware-label", required=True)
    ap.add_argument("--samples", type=int, default=65536,
                    help="fixed-capture length per repetition (larger = less relative "
                         "overhead from the 'o' query itself)")
    ap.add_argument("--reps", type=int, default=N_REPETITIONS)
    ap.add_argument("--channels", default=",".join(str(c) for c in CHANNEL_SWEEP))
    ap.add_argument("--rate-min", type=int, default=None)
    ap.add_argument("--rate-max", type=int, default=None)
    ap.add_argument("--points-per-decade", type=int, default=6)
    ap.add_argument("--output", default=None)
    args = ap.parse_args()

    channels = [int(c) for c in args.channels.split(",")]
    rate_kwargs = {}
    if args.rate_min:
        rate_kwargs["rate_min"] = args.rate_min
    if args.rate_max:
        rate_kwargs["rate_max"] = args.rate_max
    rates = log_rate_sweep(points_per_decade=args.points_per_decade, **rate_kwargs)

    out_path = args.output or (RESULTS_DIR / f"m4_{args.firmware_label}.csv")
    f, writer = csv_writer(
        out_path,
        ["firmware", "channels", "rate_hz", "rep", "busy_cycles", "total_cycles",
         "occupancy_fraction", "decode_errors"],
    )

    dbg = connect(args.port)
    try:
        # Fail fast with a clear message if this firmware predates the 'o'
        # command, instead of one confusing error per repetition.
        setup_channels(dbg, channels[0], rates[0], args.samples)
        capture_fixed_digital(dbg, channels[0])
        query_occupancy(dbg)

        for n_channels in channels:
            print(f"\n== M4: {n_channels} channel(s) ==")
            for rate_hz in rates:
                setup_channels(dbg, n_channels, rate_hz, args.samples)
                for rep in range(args.reps):
                    result = capture_fixed_digital(dbg, n_channels)
                    busy, total = query_occupancy(dbg)
                    fraction = (busy / total) if total > 0 else float("nan")

                    writer.writerow({
                        "firmware": args.firmware_label,
                        "channels": n_channels,
                        "rate_hz": rate_hz,
                        "rep": rep,
                        "busy_cycles": busy,
                        "total_cycles": total,
                        "occupancy_fraction": round(fraction, 6) if total > 0 else "",
                        "decode_errors": len(result["errors"]),
                    })
                    f.flush()
                print(f"  rate={rate_hz:>10} Hz  done ({args.reps} reps)")
    finally:
        dbg.close()
        f.close()

    print(f"\nResults written to {out_path}")


if __name__ == "__main__":
    sys.exit(main())
