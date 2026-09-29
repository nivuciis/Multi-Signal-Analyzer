# benchmarks/

Data-collection scripts for the four metrics defined in the TCC, Cap.3,
"Métricas e procedimentos experimentais" (Sec. 3.5), plus the statistical
comparison procedure from Sec. 3.6 ("Análise estatística").

These scripts run against **one firmware at a time**, over the serial/USB
CDC port. To compare Firmware A and Firmware B you run the same script
twice — once per firmware/board — tagging each run with `--firmware-label`,
then feed both CSVs to `compare.py`.

## Setup

```
pip install -r benchmarks/requirements.txt
```

`pyserial` is enough to run `m1`/`m2`/`m3`/`m4` (data collection). `numpy`
and `scipy` are only needed for `stats.py`/`compare.py` (the Sec. 3.6
analysis).

These scripts import `tools/debug_firmware.py` from this repo (protocol
implementation, stream decoder) — run them from the repo root or with
`benchmarks/` on `sys.path` (they add `../tools` themselves; no extra setup
needed beyond `cd` into the repo).

## Bench assumptions (Cap.3, "Bancada experimental")

- A waveform generator feeds a known digital pattern to the enabled
  channels — a free-running binary counter for M1/M3, a reference square
  wave (known frequency) for M2. These scripts capture and validate; they
  do not generate the stimulus.
- One host, one port, one firmware build at a time.

## What's covered vs. manual/out of scope

| Metric | Script | Fully automated? |
|---|---|---|
| M1 — taxa máxima sustentada | `m1_max_rate.py` | Yes. (Comparing B's result against the theoretical ceiling f_clk/C_iter needs C_iter from the sampling loop's disassembly — a manual, offline step, not computed here.) |
| M2 — jitter de amostragem | `m2_jitter.py` | Only the in-band half (period distribution from the decoded samples). The oscilloscope cross-check of a firmware-toggled debug pin is a separate manual bench step — the firmware has no such debug-pin instrumentation. |
| M3 — perda em captura contínua | `m3_continuous_loss.py` | Yes, via gaps in the decoded counter sequence. Firmware doesn't currently report an overflow counter over the wire, so overflow is inferred from gaps/abort, not a dedicated field. |
| M4 — ocupação de CPU | `m4_cpu_occupancy.py` | Yes, but **requires firmware built with the DWT occupancy instrumentation** (`src/occupancy.c`, `handle_get_cpu_occupancy.c`, the `'o'` command). Firmware without it will make this script fail fast with a clear error. |

## Usage

Each script sweeps `--channels` (default `1,8,12`, matching Cap.3's
CHANNEL_SWEEP) and a log-spaced rate sweep within the firmware's supported
range, `N_REPETITIONS = 30` repetitions per point by default (Cap.3, "N=30
repetições").

```
# Firmware A (main branch): flash it, then
python3 benchmarks/m1_max_rate.py --firmware-label A --port /dev/ttyACM0
python3 benchmarks/m2_jitter.py   --firmware-label A --square-channel 0
python3 benchmarks/m3_continuous_loss.py --firmware-label A --duration 60
python3 benchmarks/m4_cpu_occupancy.py   --firmware-label A

# Firmware B (ana-gpio-cpu branch): flash it, then repeat with label B
python3 benchmarks/m1_max_rate.py --firmware-label B --port /dev/ttyACM0
...

# Firmware B's dual-core / XIP-vs-SRAM / interrupts-on-off variants: same
# scripts, one firmware rebuild per variant, distinct labels, e.g.:
python3 benchmarks/m2_jitter.py --firmware-label B-sram
python3 benchmarks/m2_jitter.py --firmware-label B-xip
```

Results land in `benchmarks/results/m{1,2,3,4}_<label>.csv` (append-only —
re-running a script adds more repetitions rather than overwriting).

## Comparing A vs B (Cap.3, Sec. 3.6)

```
python3 benchmarks/compare.py results/m4_A.csv results/m4_B.csv \
    --value-column occupancy_fraction
```

`--value-column` picks the column to compare per condition
(`(channels, rate_hz)` pair): `occupancy_fraction` for M4,
`period_mean_ns`/`period_std_ns`/`period_pkpk_ns` for M2,
`effective_rate_hz`/`gap_samples_total` for M3, `ok` for M1 (though M1's
pass/fail is usually read directly off its own CSV rather than compared
statistically — a fixed max-rate value doesn't have a distribution to test
per se; the reps mainly guard against a rate that only sometimes fails).

Applies, in one call, Mann-Whitney U (always, non-parametric — no assumed
normality), rank-biserial effect size, Holm-Bonferroni correction across
every condition compared in that call (one metric = one family), and a
95% bootstrap CI (10,000 resamples) for the difference of medians.

## Known correctness note baked into these scripts

`tools/debug_firmware.py`'s `FirmwareDebugger.dig_bps` is derived once from
the device's *total* digital channel count (the identify string), which
only matches the wire format when every channel is enabled. These
benchmarks deliberately enable a **subset** (1, 8 or 12 out of however many
the board has), so they compute the wire's actual
`bytes_per_dig_sample` themselves (`common.digital_bytes_per_sample`,
mirroring the firmware's `tx_init()`) instead of trusting `dig_bps` —
see `common.capture_fixed_digital` / `common.decode_continuous_stream`.
Continuous ('C') captures also embed a `"$<n>+"` done marker once per
1,000,000-sample chunk (the `'L'` command's cap), not just once at the end
like a fixed ('F') capture — `common.decode_continuous_stream` splits on
every occurrence before decoding, instead of naively decoding the whole
raw stream in one pass.
