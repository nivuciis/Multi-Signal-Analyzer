#!/usr/bin/env python3
"""benchmarks/m1_max_rate.py — M1: Taxa máxima de amostragem sustentada
(TCC Cap.3, Sec. "Métricas e procedimentos experimentais").

Procedure: fixed-length capture of a counter pattern at increasing rate; for
each rate, validate 100% sequence integrity across N repetitions. The
sustained rate is the highest rate where every repetition is fully clean.
Swept across channel counts {1, 8, 12} (CHANNEL_SWEEP).

Note: comparing Firmware B's result against the theoretical ceiling
f_clk/C_iter (Eq. "eq:fsmax") requires C_iter from the sampling loop's
disassembly (Cap.3, Firmware B) — that step is manual/offline, not computed
by this script.

Usage:
  python3 benchmarks/m1_max_rate.py --firmware-label A --port /dev/ttyACM0
  python3 benchmarks/m1_max_rate.py --firmware-label B --samples 4096 --reps 30
"""
import argparse
import sys

from common import (
    CHANNEL_SWEEP,
    N_REPETITIONS,
    RESULTS_DIR,
    capture_fixed_digital,
    csv_writer,
    connect,
    log_rate_sweep,
    setup_channels,
    validate_counter_sequence,
)


def run_repetition(dbg, n_channels, rate_hz, n_samples):
    result = capture_fixed_digital(dbg, n_channels)
    if result["errors"] or len(result["digital"]) != n_samples:
        return False, None
    ok, first_bad = validate_counter_sequence(result["digital"], n_channels)
    return ok, first_bad


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=None)
    ap.add_argument("--firmware-label", required=True, help='e.g. "A" or "B" — tags the CSV rows')
    ap.add_argument("--samples", type=int, default=4096, help="fixed-capture length per repetition")
    ap.add_argument("--reps", type=int, default=N_REPETITIONS)
    ap.add_argument("--channels", default=",".join(str(c) for c in CHANNEL_SWEEP),
                    help="comma-separated channel counts to sweep")
    ap.add_argument("--rate-min", type=int, default=None)
    ap.add_argument("--rate-max", type=int, default=None)
    ap.add_argument("--points-per-decade", type=int, default=6)
    ap.add_argument("--no-early-stop", action="store_true",
                    help="keep sweeping past the first fully-failed rate (slower, more thorough)")
    ap.add_argument("--output", default=None, help="CSV path (default: results/m1_<label>.csv)")
    args = ap.parse_args()

    channels = [int(c) for c in args.channels.split(",")]
    rate_kwargs = {}
    if args.rate_min:
        rate_kwargs["rate_min"] = args.rate_min
    if args.rate_max:
        rate_kwargs["rate_max"] = args.rate_max
    rates = log_rate_sweep(points_per_decade=args.points_per_decade, **rate_kwargs)

    out_path = args.output or (RESULTS_DIR / f"m1_{args.firmware_label}.csv")
    f, writer = csv_writer(
        out_path,
        ["firmware", "channels", "rate_hz", "rep", "ok", "first_bad_index"],
    )

    dbg = connect(args.port)
    try:
        for n_channels in channels:
            print(f"\n== M1: {n_channels} channel(s) ==")
            max_sustained = None
            for rate_hz in rates:
                setup_channels(dbg, n_channels, rate_hz, args.samples)
                passes = 0
                for rep in range(args.reps):
                    ok, first_bad = run_repetition(dbg, n_channels, rate_hz, args.samples)
                    writer.writerow({
                        "firmware": args.firmware_label,
                        "channels": n_channels,
                        "rate_hz": rate_hz,
                        "rep": rep,
                        "ok": int(ok),
                        "first_bad_index": "" if first_bad is None else first_bad,
                    })
                    f.flush()
                    passes += int(ok)
                all_ok = passes == args.reps
                print(f"  rate={rate_hz:>10} Hz  {passes:>2}/{args.reps} clean"
                      f"  {'OK' if all_ok else 'FAIL'}")
                if all_ok:
                    max_sustained = rate_hz
                elif not args.no_early_stop:
                    print("  (stopping sweep for this channel count — first failed rate)")
                    break
            print(f"  -> max sustained rate: {max_sustained} Hz")
    finally:
        dbg.close()
        f.close()

    print(f"\nResults written to {out_path}")


if __name__ == "__main__":
    sys.exit(main())
