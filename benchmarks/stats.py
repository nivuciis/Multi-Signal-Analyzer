"""benchmarks/stats.py — statistical comparison procedure (TCC Cap.3, Sec.
"Análise estatística").

Fixed, a-priori procedure applied to every A vs. B comparison, not chosen
ad hoc per metric:
  - Mann-Whitney U (scipy.stats.mannwhitneyu), always applied.
  - Effect size: rank-biserial correlation (equivalent to Cliff's delta).
  - Holm-Bonferroni correction across the comparisons within one metric's
    family (M1, M2, M3, M4 each their own family).
  - 95% CI for the difference of medians via percentile bootstrap
    (10,000 resamples).

Requires scipy and numpy (not needed by the data-collection scripts in this
directory) — see benchmarks/requirements.txt.
"""
from dataclasses import dataclass

import numpy as np
from scipy import stats as scipy_stats


@dataclass
class ComparisonResult:
    condition: str
    n_a: int
    n_b: int
    median_a: float
    median_b: float
    u_statistic: float
    p_value: float
    p_value_adjusted: float | None
    effect_size_rank_biserial: float
    ci95_median_diff: tuple


def rank_biserial_effect_size(a, b, u_statistic):
    """Rank-biserial correlation from the Mann-Whitney U statistic.
    r = 1 - 2U / (n_a * n_b); equivalent to Cliff's delta."""
    n_a, n_b = len(a), len(b)
    return 1.0 - (2.0 * u_statistic) / (n_a * n_b)


def bootstrap_median_diff_ci(a, b, n_resamples=10_000, ci=0.95, rng=None):
    """Percentile bootstrap CI for median(b) - median(a)."""
    rng = rng or np.random.default_rng()
    a = np.asarray(a)
    b = np.asarray(b)
    diffs = np.empty(n_resamples)
    for i in range(n_resamples):
        ra = rng.choice(a, size=len(a), replace=True)
        rb = rng.choice(b, size=len(b), replace=True)
        diffs[i] = np.median(rb) - np.median(ra)
    lo = (1 - ci) / 2 * 100
    hi = (1 + ci) / 2 * 100
    return float(np.percentile(diffs, lo)), float(np.percentile(diffs, hi))


def compare_condition(condition_label, sample_a, sample_b, n_resamples=10_000, rng=None):
    """One A-vs-B comparison for a single condition (e.g. one rate x channel
    count point). p_value_adjusted is left None here — call
    holm_bonferroni() across all conditions in the same metric family
    afterwards."""
    a = np.asarray(sample_a, dtype=float)
    b = np.asarray(sample_b, dtype=float)

    result = scipy_stats.mannwhitneyu(a, b, alternative="two-sided")
    effect = rank_biserial_effect_size(a, b, result.statistic)
    ci = bootstrap_median_diff_ci(a, b, n_resamples=n_resamples, rng=rng)

    return ComparisonResult(
        condition=condition_label,
        n_a=len(a),
        n_b=len(b),
        median_a=float(np.median(a)),
        median_b=float(np.median(b)),
        u_statistic=float(result.statistic),
        p_value=float(result.pvalue),
        p_value_adjusted=None,
        effect_size_rank_biserial=effect,
        ci95_median_diff=ci,
    )


def holm_bonferroni(results):
    """Apply the Holm-Bonferroni correction in place (sets
    p_value_adjusted) across `results` (one metric family, e.g. all M1
    rate x channel-count conditions). Returns the same list, sorted back to
    the original order."""
    indexed = sorted(enumerate(results), key=lambda pair: pair[1].p_value)
    m = len(indexed)
    running_max = 0.0
    for rank, (orig_idx, res) in enumerate(indexed):
        adjusted = (m - rank) * res.p_value
        running_max = max(running_max, adjusted)
        res.p_value_adjusted = min(1.0, running_max)
    return results
