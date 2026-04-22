#!/usr/bin/env python3
"""Generate A/B vehicle-shadowing comparison plots from secondary DT Redis streams.

Expected Redis keys:
- dt2:link:v2v:{run_id}:{tx_id}:{rx_id}:samples
- dt2:link:v2rsu:{run_id}:{tx_id}:{rx_id}:samples

Default run IDs match omnetpp.ini configs added for this experiment.
"""

from __future__ import annotations

import argparse
import math
import os
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

try:
    import redis
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Install redis-py first: pip install redis") from exc

try:
    import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Install matplotlib first: pip install matplotlib") from exc


TX_POWER_DBM = 33.0
NOISE_DBM = -110.0
PATHLOSS_ALPHA = 2.2
FREQ_HZ = 5.89e9
C_LIGHT = 299792458.0


@dataclass
class Sample:
    run_mode: str
    link_type: str
    tx_id: str
    rx_id: str
    cycle_index: int
    step_index: int
    predicted_time: float
    distance_m: float
    sinr_db: float

    @property
    def effective_path_loss_db(self) -> float:
        # With interference disabled, SINR approximates SNR so we can back-calc
        # effective total path loss (distance + static obstacle + shadowing terms).
        rx_dbm = NOISE_DBM + self.sinr_db
        return TX_POWER_DBM - rx_dbm

    @property
    def fspl_path_loss_db(self) -> float:
        d_m = max(1.0, self.distance_m)
        wavelength = C_LIGHT / FREQ_HZ
        return 20.0 * math.log10(4.0 * math.pi / wavelength) + 10.0 * PATHLOSS_ALPHA * math.log10(d_m)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot secondary shadowing A/B comparison")
    parser.add_argument("--redis-host", default="127.0.0.1")
    parser.add_argument("--redis-port", type=int, default=6379)
    parser.add_argument("--redis-db", type=int, default=3)
    parser.add_argument("--with-run-id", default="DT-Secondary-Shadowing-With")
    parser.add_argument("--without-run-id", default="DT-Secondary-Shadowing-Without")
    parser.add_argument("--output-dir", default="plots")

    parser.add_argument("--v2v-tx", default="")
    parser.add_argument("--v2v-rx", default="")
    parser.add_argument("--v2r-tx", default="")
    parser.add_argument("--v2r-rx", default="")

    parser.add_argument("--max-stream-entries", type=int, default=300000)
    parser.add_argument("--payload-bytes", type=float, default=500000.0)
    parser.add_argument("--bandwidth-hz", type=float, default=10e6)
    parser.add_argument("--efficiency", type=float, default=0.8)
    parser.add_argument("--rate-cap-bps", type=float, default=-1.0)
    return parser.parse_args()


def safe_float(value: object, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def safe_int(value: object, default: int = 0) -> int:
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def _parse_link_key(key: str) -> Optional[Tuple[str, str]]:
    """Extract (tx_id, rx_id) from dt2:link:{type}:{run}:{tx}:{rx}:samples."""
    if not key.endswith(":samples"):
        return None
    parts = key.split(":")
    if len(parts) < 7:
        return None
    return parts[-3], parts[-2]


def scan_pairs(
    redis_client: redis.Redis,
    run_id: str,
    link_str: str,
) -> Dict[Tuple[str, str], str]:
    """Return {(tx_id, rx_id): redis_key} for all pair streams of a link type and run."""
    result: Dict[Tuple[str, str], str] = {}
    for key in redis_client.keys(f"dt2:link:{link_str}:{run_id}:*"):
        pair = _parse_link_key(key)
        if pair:
            result[pair] = key
    return result


def _fspl_sinr(dist_m: float) -> float:
    wavelength = C_LIGHT / FREQ_HZ
    fspl_db = (
        20.0 * math.log10(4.0 * math.pi / wavelength)
        + 10.0 * PATHLOSS_ALPHA * math.log10(max(1.0, dist_m))
    )
    return TX_POWER_DBM - fspl_db - NOISE_DBM


def load_link_samples(
    redis_client: redis.Redis,
    key: str,
    run_mode: str,
    link_type: str,
    tx_id: str,
    rx_id: str,
    max_entries: int,
) -> List[Sample]:
    """Load samples from one link stream.

    Uses sinr_db from the stream when present (written by the simulation after
    the C++ fix); falls back to FSPL-based computation for legacy data.
    """
    if not key or not redis_client.exists(key):
        return []
    samples: List[Sample] = []
    for idx, (_, fields) in enumerate(
        redis_client.xrange(key, min="-", max="+", count=max_entries)
    ):
        dist_m = max(1.0, safe_float(fields.get("distance_m"), 1.0))
        sim_time = safe_float(fields.get("sim_time"))
        raw_sinr = fields.get("sinr_db")
        sinr_db = safe_float(raw_sinr) if raw_sinr is not None else _fspl_sinr(dist_m)
        samples.append(
            Sample(
                run_mode=run_mode,
                link_type=link_type,
                tx_id=tx_id,
                rx_id=rx_id,
                cycle_index=0,
                step_index=idx,
                predicted_time=sim_time,
                distance_m=dist_m,
                sinr_db=sinr_db,
            )
        )
    samples.sort(key=lambda s: s.predicted_time)
    return samples


def choose_common_pair(
    redis_client: redis.Redis,
    with_map: Dict[Tuple[str, str], str],
    without_map: Dict[Tuple[str, str], str],
    forced_tx: str,
    forced_rx: str,
) -> Tuple[str, str]:
    if forced_tx and forced_rx:
        return forced_tx, forced_rx
    common = set(with_map) & set(without_map)
    if not common:
        raise RuntimeError("No link pairs appear in both With and Without runs")
    return max(
        common,
        key=lambda p: redis_client.xlen(with_map[p]) + redis_client.xlen(without_map[p]),
    )


def effective_rate_bps(sinr_db: float, bandwidth_hz: float, efficiency: float, cap_bps: float) -> float:
    safe_bw = max(1.0, bandwidth_hz)
    safe_eff = max(0.01, min(1.0, efficiency))
    sinr_linear = math.pow(10.0, sinr_db / 10.0)
    rate = safe_eff * safe_bw * math.log2(1.0 + max(0.0, sinr_linear))
    if cap_bps > 0.0:
        rate = min(rate, cap_bps)
    return max(1.0, rate)


def tx_time_seconds(payload_bytes: float, rate_bps: float) -> float:
    return max(0.0, payload_bytes) * 8.0 / max(1.0, rate_bps)


def cdf_xy(values: List[float]) -> Tuple[List[float], List[float]]:
    vals = sorted(values)
    n = len(vals)
    if n == 0:
        return [], []
    xs = vals
    ys = [(i + 1) / n for i in range(n)]
    return xs, ys


def plot_time_series(
    out_path: str,
    title: str,
    y_label: str,
    with_times: List[float],
    with_vals: List[float],
    without_times: List[float],
    without_vals: List[float],
) -> None:
    plt.figure(figsize=(12, 5), dpi=140)
    plt.plot(with_times, with_vals, label="With vehicle shadowing", linewidth=1.2)
    plt.plot(without_times, without_vals, label="Without vehicle shadowing", linewidth=1.2)
    plt.title(title)
    plt.xlabel("Predicted time (s)")
    plt.ylabel(y_label)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def main() -> None:
    args = parse_args()
    os.makedirs(args.output_dir, exist_ok=True)

    r = redis.Redis(host=args.redis_host, port=args.redis_port, db=args.redis_db, decode_responses=True)
    try:
        r.ping()
    except Exception as exc:
        raise SystemExit(f"Redis connection failed: {exc}") from exc

    v2v_with_map = scan_pairs(r, args.with_run_id, "v2v")
    v2v_without_map = scan_pairs(r, args.without_run_id, "v2v")
    v2r_with_map = scan_pairs(r, args.with_run_id, "v2rsu")
    v2r_without_map = scan_pairs(r, args.without_run_id, "v2rsu")

    if not v2v_with_map and not v2v_without_map:
        raise SystemExit("No V2V link streams found for either run")
    if not v2r_with_map and not v2r_without_map:
        raise SystemExit("No V2R link streams found for either run")

    v2v_tx, v2v_rx = choose_common_pair(r, v2v_with_map, v2v_without_map, args.v2v_tx, args.v2v_rx)
    v2r_tx, v2r_rx = choose_common_pair(r, v2r_with_map, v2r_without_map, args.v2r_tx, args.v2r_rx)

    print(f"Selected V2V pair: {v2v_tx} -> {v2v_rx}")
    print(f"Selected V2R pair: {v2r_tx} -> {v2r_rx}")

    v2v_with = load_link_samples(r, v2v_with_map.get((v2v_tx, v2v_rx), ""), "with_shadowing", "V2V", v2v_tx, v2v_rx, args.max_stream_entries)
    v2v_without = load_link_samples(r, v2v_without_map.get((v2v_tx, v2v_rx), ""), "without_shadowing", "V2V", v2v_tx, v2v_rx, args.max_stream_entries)
    v2r_with = load_link_samples(r, v2r_with_map.get((v2r_tx, v2r_rx), ""), "with_shadowing", "V2RSU", v2r_tx, v2r_rx, args.max_stream_entries)
    v2r_without = load_link_samples(r, v2r_without_map.get((v2r_tx, v2r_rx), ""), "without_shadowing", "V2RSU", v2r_tx, v2r_rx, args.max_stream_entries)

    if not v2v_with or not v2v_without:
        raise SystemExit("Insufficient V2V samples for selected pair")
    if not v2r_with or not v2r_without:
        raise SystemExit("Insufficient V2R samples for selected pair")

    def series(samples: List[Sample]) -> Tuple[List[float], List[float], List[float], List[float]]:
        ts = [s.predicted_time for s in samples]
        pathloss = [s.effective_path_loss_db for s in samples]
        sinr = [s.sinr_db for s in samples]
        rate_mbps = [effective_rate_bps(s.sinr_db, args.bandwidth_hz, args.efficiency, args.rate_cap_bps) / 1e6 for s in samples]
        return ts, pathloss, sinr, rate_mbps

    v2v_t_with, v2v_pl_with, v2v_sinr_with, v2v_rate_with = series(v2v_with)
    v2v_t_without, v2v_pl_without, v2v_sinr_without, v2v_rate_without = series(v2v_without)
    v2r_t_with, v2r_pl_with, v2r_sinr_with, v2r_rate_with = series(v2r_with)
    v2r_t_without, v2r_pl_without, v2r_sinr_without, v2r_rate_without = series(v2r_without)

    plot_time_series(
        os.path.join(args.output_dir, "v2v_pathloss_vs_time.png"),
        "V2V Effective Path Loss vs Time",
        "Effective path loss (dB)",
        v2v_t_with,
        v2v_pl_with,
        v2v_t_without,
        v2v_pl_without,
    )
    plot_time_series(
        os.path.join(args.output_dir, "v2v_sinr_vs_time.png"),
        "V2V SINR vs Time",
        "SINR (dB)",
        v2v_t_with,
        v2v_sinr_with,
        v2v_t_without,
        v2v_sinr_without,
    )
    plot_time_series(
        os.path.join(args.output_dir, "v2v_rate_vs_time.png"),
        "V2V Data Rate vs Time",
        "Data rate (Mbps)",
        v2v_t_with,
        v2v_rate_with,
        v2v_t_without,
        v2v_rate_without,
    )

    plot_time_series(
        os.path.join(args.output_dir, "v2r_pathloss_vs_time.png"),
        "V2R Effective Path Loss vs Time",
        "Effective path loss (dB)",
        v2r_t_with,
        v2r_pl_with,
        v2r_t_without,
        v2r_pl_without,
    )
    plot_time_series(
        os.path.join(args.output_dir, "v2r_sinr_vs_time.png"),
        "V2R SINR vs Time",
        "SINR (dB)",
        v2r_t_with,
        v2r_sinr_with,
        v2r_t_without,
        v2r_sinr_without,
    )
    plot_time_series(
        os.path.join(args.output_dir, "v2r_rate_vs_time.png"),
        "V2R Data Rate vs Time",
        "Data rate (Mbps)",
        v2r_t_with,
        v2r_rate_with,
        v2r_t_without,
        v2r_rate_without,
    )

    # Combined SINR CDF
    plt.figure(figsize=(8, 5), dpi=140)
    for label, values in [
        ("With V2V", v2v_sinr_with),
        ("Without V2V", v2v_sinr_without),
        ("With V2R", v2r_sinr_with),
        ("Without V2R", v2r_sinr_without),
    ]:
        xs, ys = cdf_xy(values)
        plt.plot(xs, ys, label=label, linewidth=1.3)
    plt.title("SINR CDF Comparison")
    plt.xlabel("SINR (dB)")
    plt.ylabel("CDF")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(args.output_dir, "sinr_cdf_compare.png"))
    plt.close()

    # Transmission-time comparison from SINR-derived rates
    tx_groups = []
    tx_labels = []
    for label, rates_mbps in [
        ("With V2V", v2v_rate_with),
        ("Without V2V", v2v_rate_without),
        ("With V2R", v2r_rate_with),
        ("Without V2R", v2r_rate_without),
    ]:
        rates_bps = [r * 1e6 for r in rates_mbps]
        tx_groups.append([tx_time_seconds(args.payload_bytes, rb) * 1000.0 for rb in rates_bps])
        tx_labels.append(label)

    plt.figure(figsize=(9, 5), dpi=140)
    plt.boxplot(tx_groups, labels=tx_labels, showfliers=False)
    plt.title("Transmission Time Comparison")
    plt.ylabel("Tx time (ms)")
    plt.grid(True, axis="y", alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(args.output_dir, "tx_time_compare.png"))
    plt.close()

    print(f"Generated plots in: {args.output_dir}")


if __name__ == "__main__":
    main()
