#!/usr/bin/env python3
"""benchmarks/compare.py — apply the Cap.3 "Análise estatística" procedure
(Mann-Whitney U + rank-biserial effect size + Holm-Bonferroni + bootstrap CI)
to a pair of Firmware A / Firmware B result CSVs from m1..m4.

Each condition is one (channels, rate_hz) group; all conditions in one
metric's CSV pair form one Holm-Bonferroni family, per Cap.3.

Usage:
  python3 benchmarks/compare.py results/m4_A.csv results/m4_B.csv \\
      --value-column occupancy_fraction --output results/m4_compare.csv
"""
import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path

from stats import compare_condition, holm_bonferroni


def load_grouped(path, value_column):
    groups = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if row.get(value_column, "") == "":
                continue
            key = (row["channels"], row["rate_hz"])
            groups[key].append(float(row[value_column]))
    return groups


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv_a", help="Firmware A results CSV (e.g. results/m4_A.csv)")
    ap.add_argument("csv_b", help="Firmware B results CSV (e.g. results/m4_B.csv)")
    ap.add_argument("--value-column", required=True,
                    help="column to compare, e.g. occupancy_fraction, "
                         "period_mean_ns, effective_rate_hz")
    ap.add_argument("--output", default=None)
    args = ap.parse_args()

    groups_a = load_grouped(args.csv_a, args.value_column)
    groups_b = load_grouped(args.csv_b, args.value_column)

    common_keys = sorted(set(groups_a) & set(groups_b))
    missing_a = set(groups_b) - set(groups_a)
    missing_b = set(groups_a) - set(groups_b)
    if missing_a:
        print(f"warning: {len(missing_a)} condition(s) in B but not A, skipped", file=sys.stderr)
    if missing_b:
        print(f"warning: {len(missing_b)} condition(s) in A but not B, skipped", file=sys.stderr)

    results = []
    for channels, rate_hz in common_keys:
        label = f"channels={channels} rate_hz={rate_hz}"
        results.append(compare_condition(label, groups_a[(channels, rate_hz)],
                                         groups_b[(channels, rate_hz)]))

    holm_bonferroni(results)

    out_path = Path(args.output) if args.output else Path(args.csv_a).with_name(
        Path(args.csv_a).stem.rsplit("_", 1)[0] + "_compare.csv")
    with open(out_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["condition", "n_a", "n_b", "median_a", "median_b", "u_statistic",
                         "p_value", "p_value_adjusted", "effect_size_rank_biserial",
                         "ci95_diff_low", "ci95_diff_high"])
        for r in results:
            writer.writerow([r.condition, r.n_a, r.n_b, r.median_a, r.median_b,
                             r.u_statistic, r.p_value, r.p_value_adjusted,
                             r.effect_size_rank_biserial, r.ci95_median_diff[0],
                             r.ci95_median_diff[1]])
            print(f"{r.condition:35s} p={r.p_value:.4g} p_adj={r.p_value_adjusted:.4g} "
                  f"effect={r.effect_size_rank_biserial:+.3f} "
                  f"median A={r.median_a:.4g} B={r.median_b:.4g} "
                  f"CI95(B-A)=[{r.ci95_median_diff[0]:.4g}, {r.ci95_median_diff[1]:.4g}]")

    print(f"\nWritten to {out_path}")


if __name__ == "__main__":
    sys.exit(main())
