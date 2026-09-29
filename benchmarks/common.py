"""benchmarks/common.py — shared helpers for the M1-M4 experiments (TCC Cap.3,
"Métricas e procedimentos experimentais").

Reuses the protocol implementation already in tools/debug_firmware.py
(FirmwareDebugger, decode_stream, find_port) instead of re-parsing the
srpico wire format here; these scripts only add the experiment procedures
(sweeps, repetitions, integrity/gap checks) and CSV output on top of it.

Assumes a counter-pattern stimulus is already being fed to the enabled
digital channels by the bench's waveform generator (Cap.3, "Bancada
experimental") — these scripts capture and validate, they do not generate
the stimulus.
"""
import csv
import sys
import time
from pathlib import Path

BENCHMARKS_DIR = Path(__file__).resolve().parent
TOOLS_DIR = BENCHMARKS_DIR.parent / "tools"
RESULTS_DIR = BENCHMARKS_DIR / "results"

if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from debug_firmware import (  # noqa: E402
    DONE_MARKER_RE,
    FirmwareDebugger,
    decode_stream,
    find_port,
)


# Cap.3, Sec. "Métricas e procedimentos experimentais": channel counts swept
# for every metric, and repetitions per measurement point.
CHANNEL_SWEEP = (1, 8, 12)
N_REPETITIONS = 30

# Cap.3, Firmware A/B command table: full sigrok-pico rate range.
RATE_MIN_HZ = 5_000
RATE_MAX_HZ = 120_000_000


def log_rate_sweep(rate_min=RATE_MIN_HZ, rate_max=RATE_MAX_HZ, points_per_decade=6):
    """Sample rates in ascending logarithmic steps within [rate_min, rate_max],
    rounded to whole Hz, de-duplicated. Matches "varre-se a taxa de amostragem
    em passos logarítmicos" (Cap.3)."""
    import math

    decades = math.log10(rate_max / rate_min)
    n_points = max(2, int(decades * points_per_decade) + 1)
    rates = []
    for i in range(n_points):
        r = rate_min * (rate_max / rate_min) ** (i / (n_points - 1))
        rates.append(int(round(r)))
    # de-duplicate while preserving order
    seen = set()
    out = []
    for r in rates:
        if r not in seen:
            seen.add(r)
            out.append(r)
    return out


def connect(port=None) -> FirmwareDebugger:
    port = port or find_port()
    if not port:
        raise SystemExit(
            "No serial port found. Pass --port explicitly, e.g. --port /dev/ttyACM0"
        )
    return FirmwareDebugger(port)


def setup_channels(dbg: FirmwareDebugger, n_channels: int, rate_hz: int, n_samples: int):
    """Reset the device and enable exactly D0..D(n_channels-1), no analog,
    no trigger, at the given rate/sample-count."""
    dbg.reset()
    dbg.identify()
    dbg.disable_all_analog()
    dbg.clear_trigger()
    dbg.set_sample_rate(rate_hz)
    dbg.set_sample_limit(n_samples)
    for ch in range(dbg.n_digital):
        dbg.enable_digital(ch, ch < n_channels)


def validate_counter_sequence(samples, n_bits):
    """Check that `samples` (already masked to n_bits) is a free-running
    binary counter with no missing/duplicated steps.

    @return (ok, first_bad_index) — first_bad_index is None when ok.
    """
    if not samples:
        return False, None
    modulus = 1 << n_bits
    prev = samples[0] & (modulus - 1)
    for i in range(1, len(samples)):
        cur = samples[i] & (modulus - 1)
        expected = (prev + 1) % modulus
        if cur != expected:
            return False, i
        prev = cur
    return True, None


def find_counter_gaps(samples, n_bits):
    """Like validate_counter_sequence, but keeps scanning and reports every
    gap instead of stopping at the first one (Cap.3, M3: "número e tamanho
    das lacunas via quebras na sequência do contador").

    @return list of {'index': int, 'size': int} — size = samples actually
    missing between the two observed values (0 for a plain duplicate).
    """
    if not samples:
        return []
    modulus = 1 << n_bits
    gaps = []
    prev = samples[0] & (modulus - 1)
    for i in range(1, len(samples)):
        cur = samples[i] & (modulus - 1)
        expected = (prev + 1) % modulus
        if cur != expected:
            missing = (cur - expected) % modulus
            gaps.append({"index": i, "size": missing})
        prev = cur
    return gaps


def digital_bytes_per_sample(n_channels: int) -> int:
    """Bytes needed to carry n_channels digital bits on the wire, matching
    the firmware's tx_init(): bytes_per_dig_sample = (highest_bit // 7) + 1,
    where highest_bit = n_channels - 1 for a contiguous D0..D(n-1) mask.

    NOTE: FirmwareDebugger.dig_bps (from tools/debug_firmware.py) is instead
    derived once from the device's *total* digital channel count (the
    identify string), which only matches the wire format when every
    channel is enabled. These benchmarks deliberately enable a subset
    (CHANNEL_SWEEP = 1/8/12), so callers must use THIS value — not
    dbg.dig_bps — when decoding a subset capture (see capture_fixed_digital
    below).
    """
    if n_channels <= 0:
        return 0
    return ((n_channels - 1) // 7) + 1


def capture_fixed_digital(dbg: FirmwareDebugger, n_channels: int, timeout=10.0):
    """Run a fixed ('F') capture and decode it as a digital-only, n_channels
    stream, using the correct subset-aware dig_bps (see
    digital_bytes_per_sample) instead of FirmwareDebugger.fixed_capture's
    fixed, total-channel-count-based one.

    @return the same result dict as debug_firmware.decode_stream().
    """
    dig_bps = digital_bytes_per_sample(n_channels)
    dbg._send(b"F\n")
    raw = dbg._read_stream(timeout=timeout)
    return decode_stream(raw, dig_bps, ana_bps=0)


def decode_continuous_stream(raw: bytes, dig_bps: int):
    """Decode a continuous ('C') capture's raw stream.

    Continuous mode re-arms and re-sends a "$<n>+" done marker once per
    firmware-side chunk (capped at SIGROK_SAMPLE_LIMIT_MAX = 1,000,000
    samples by the 'L' command), so a multi-second capture embeds several
    of these markers mid-stream — not just one at the end, as F captures
    do. decode_stream() only strips the LAST marker; any earlier one would
    have its ASCII digit bytes (0x30-0x39) misread as spurious RLE count
    bytes (RLE range is 0x30-0x7F). This splits the raw stream at every
    marker and decodes each segment independently before concatenating.

    A trailing segment with no marker (the capture was stopped by '+'
    before its chunk finished — the firmware sends no marker at all for a
    host-initiated stop) is decoded too; it may report a truncation error
    for a sample cut off mid-byte, which is an expected edge effect of
    stopping at an arbitrary point in time, not a real integrity issue.

    @return dict with 'digital' (concatenated samples), 'errors'
    (concatenated, tagged per segment), 'total_sent' (sum of each
    segment's reported byte count), 'n_segments'.
    """
    digital = []
    errors = []
    total_sent = 0
    start = 0
    n_segments = 0

    for m in DONE_MARKER_RE.finditer(raw):
        segment = raw[start:m.end()]
        result = decode_stream(segment, dig_bps, ana_bps=0)
        digital.extend(result["digital"])
        errors.extend(f"segment {n_segments}: {e}" for e in result["errors"])
        total_sent += result["total_sent"]
        start = m.end()
        n_segments += 1

    tail = raw[start:]
    if tail:
        result = decode_stream(tail, dig_bps, ana_bps=0)
        digital.extend(result["digital"])
        errors.extend(f"tail segment: {e}" for e in result["errors"])
        n_segments += 1

    return {
        "digital": digital,
        "errors": errors,
        "total_sent": total_sent,
        "n_segments": n_segments,
    }


def query_occupancy(dbg: FirmwareDebugger):
    """Send the 'o' command (added alongside occupancy.c/handle_get_cpu_occupancy.c)
    and parse the "<busy_cycles>/<total_cycles>" response.

    Requires firmware built with the DWT occupancy instrumentation — see
    src/occupancy.c and the 'o' entry in sigrok_commands[]. Raises
    RuntimeError if the firmware doesn't understand 'o' (no response before
    timeout, or a bare '*' with nothing else).
    """
    dbg._send(b"o\n")
    resp = b""
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline:
        c = dbg.ser.read(1)
        if c:
            resp += c
            if resp.endswith(b"*"):
                break
    text = resp.rstrip(b"*").strip().decode(errors="replace")
    if "/" not in text:
        raise RuntimeError(
            f"no occupancy data in response {resp!r} — is this firmware built "
            f"with the DWT occupancy instrumentation (src/occupancy.c)?"
        )
    busy_str, total_str = text.split("/", 1)
    return int(busy_str), int(total_str)


def csv_writer(path: Path, fieldnames):
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    is_new = not path.exists()
    f = open(path, "a", newline="")
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    if is_new:
        writer.writeheader()
    return f, writer
