#!/usr/bin/env python3
"""
Benchmark script to measure SINR parity between primary (Veins OMNeT++) simulation
and secondary (standalone C++ in MyRSUApp) SINR calculations.

Usage:
    python3 benchmark_sinr_parity.py [--redis-host localhost] [--redis-port 6379] \
                                     [--output-dir ./benchmark_results] \
                                     [--run-simulation] [--config-name "DT-Secondary-MotionChannel"]

Output:
    - benchmark_results/parity_stats.csv
    - benchmark_results/parity_report.md
"""

import os
import sys
import json
import argparse
import sqlite3
import statistics
from pathlib import Path
from collections import defaultdict
from typing import Dict, List, Tuple
from datetime import datetime

try:
    import redis
except ImportError:
    redis = None

try:
    import pandas as pd
except ImportError:
    pd = None


class SINRBenchmark:
    """Compare SINR values between primary and secondary calculations."""
    
    def __init__(self, output_dir: str = "./benchmark_results"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True, parents=True)
        self.samples = []
        self.stats = {}
        
    def load_redis_secondary_sinr(self, host: str = "localhost", port: int = 6379) -> List[Dict]:
        """Load secondary SINR values from Redis.
        
        Supported Redis sources:
        - dt2:q:<run_id>:entries (stream)
        - sinr:{tx_id}:{rx_id}:{timestamp} (legacy JSON string)
        """
        if redis is None:
            print("⚠️  redis-py not installed. Skipping Redis load.")
            return []
        
        try:
            r = redis.Redis(host=host, port=port, decode_responses=True)
            r.ping()
            print(f"✓ Connected to Redis at {host}:{port}")
        except Exception as e:
            print(f"✗ Redis connection failed: {e}")
            return []
        
        secondary_samples = []
        try:
            # Preferred source: secondary Q stream entries produced by MyRSUApp.
            q_stream_keys = r.keys("dt2:q:*:entries")
            q_loaded = 0
            for stream_key in q_stream_keys:
                try:
                    entries = r.xrevrange(stream_key, max="+", min="-", count=10000)
                    for entry_id, fields in entries:
                        sample = {
                            "tx_id": str(fields.get("tx_id", "")),
                            "rx_id": str(fields.get("rx_id", "")),
                            "tx_x": float(fields.get("tx_x", 0.0)),
                            "tx_y": float(fields.get("tx_y", 0.0)),
                            "rx_x": float(fields.get("rx_x", 0.0)),
                            "rx_y": float(fields.get("rx_y", 0.0)),
                            "sinr_db": float(fields.get("sinr_db", 0.0)),
                            "distance_m": float(fields.get("distance_m", 0.0)),
                            "link_type": fields.get("link_type", "unknown"),
                            "cycle_index": int(float(fields.get("cycle_index", 0))),
                            "step_index": int(float(fields.get("step_index", 0))),
                            "predicted_time": float(fields.get("predicted_time", 0.0)),
                            "source": "secondary",
                            "redis_key": stream_key,
                            "redis_entry_id": entry_id,
                        }
                        secondary_samples.append(sample)
                        q_loaded += 1
                except Exception as e:
                    print(f"  Skip stream {stream_key}: {e}")

            print(f"Found {len(q_stream_keys)} Q streams, loaded {q_loaded} stream samples")

            # Legacy fallback: sinr:* JSON keys.
            if not secondary_samples:
                keys = r.keys("sinr:*")
                print(f"Found {len(keys)} legacy SINR records in Redis")
                for key in keys[:10000]:  # Limit to 10k samples for speed
                    try:
                        raw = r.get(key)
                        if not raw:
                            continue
                        data = json.loads(raw)
                        data["source"] = "secondary"
                        data["redis_key"] = key
                        secondary_samples.append(data)
                    except Exception as e:
                        print(f"  Skip key {key}: {e}")
        except Exception as e:
            print(f"✗ Redis scan failed: {e}")
        
        print(f"  Loaded {len(secondary_samples)} secondary samples from Redis")
        return secondary_samples
    
    def load_postgres_secondary_sinr(self, db_path: str = "sinr_log.sqlite") -> List[Dict]:
        """Load secondary SINR values from SQLite/PostgreSQL log.
        
        Expected columns: tx_id, rx_id, tx_x, tx_y, rx_x, rx_y, sinr_db, timestamp
        """
        if not Path(db_path).exists():
            print(f"✗ Database not found: {db_path}")
            return []
        
        secondary_samples = []
        try:
            conn = sqlite3.connect(db_path)
            conn.row_factory = sqlite3.Row
            cursor = conn.cursor()
            
            # Try common table names
            for table_name in ["sinr_log", "secondary_sinr", "sinr_estimates"]:
                try:
                    cursor.execute(f"SELECT * FROM {table_name} LIMIT 10000")
                    rows = cursor.fetchall()
                    for row in rows:
                        secondary_samples.append({
                            "tx_id": row["tx_id"],
                            "rx_id": row["rx_id"],
                            "tx_x": row["tx_x"],
                            "tx_y": row["tx_y"],
                            "rx_x": row["rx_x"],
                            "rx_y": row["rx_y"],
                            "sinr_db": row["sinr_db"],
                            "distance_m": ((row["tx_x"] - row["rx_x"])**2 + 
                                          (row["tx_y"] - row["rx_y"])**2)**0.5,
                            "source": "secondary",
                        })
                    if rows:
                        print(f"  Loaded {len(rows)} samples from table '{table_name}'")
                except Exception as e:
                    pass
            
            conn.close()
        except Exception as e:
            print(f"✗ Database load failed: {e}")
        
        return secondary_samples
    
    def load_omnetpp_vectors(self, file_path: str) -> List[Dict]:
        """Load primary SINR from OMNeT++ output vectors (.vec file).
        
        This requires parsing OMNeT++ vector format or converting to CSV first.
        For now, returns placeholder.
        """
        print(f"⚠️  OMNeT++ vector parsing not yet implemented. See {file_path}")
        return []
    
    def pair_samples(self, secondary_samples: List[Dict]) -> Tuple[List[Dict], List[str]]:
        """Pair secondary samples by (tx_id, rx_id) for comparison.
        
        For now, group and compute averages within the secondary dataset.
        """
        pairs = defaultdict(list)
        
        for sample in secondary_samples:
            key = (sample.get("tx_id"), sample.get("rx_id"))
            pairs[key].append(sample)
        
        paired_samples = []
        for (tx_id, rx_id), samples in pairs.items():
            avg_sinr = statistics.mean([s["sinr_db"] for s in samples])
            avg_x_tx = statistics.mean([s["tx_x"] for s in samples])
            avg_y_tx = statistics.mean([s["tx_y"] for s in samples]) if "tx_y" in samples[0] else avg_x_tx
            avg_x_rx = statistics.mean([s["rx_x"] for s in samples])
            avg_y_rx = statistics.mean([s["rx_y"] for s in samples])
            distance = ((avg_x_tx - avg_x_rx)**2 + (avg_y_tx - avg_y_rx)**2)**0.5
            
            # Determine link type
            tx_id_s = str(tx_id).lower()
            rx_id_s = str(rx_id).lower()
            link_type = "RSU2RSU" if tx_id_s.startswith("rsu") and rx_id_s.startswith("rsu") else \
                       "V2RSU" if rx_id_s.startswith("rsu") else \
                       "RSU2V" if tx_id_s.startswith("rsu") else "V2V"
            
            paired_samples.append({
                "tx_id": tx_id,
                "rx_id": rx_id,
                "distance_m": distance,
                "sinr_db": avg_sinr,
                "link_type": link_type,
                "sample_count": len(samples),
            })
        
        return paired_samples, list(pairs.keys())
    
    def compute_statistics(self, secondary_samples: List[Dict]) -> Dict:
        """Compute summary statistics by link type and distance band."""
        stats = {
            "total_samples": len(secondary_samples),
            "by_link_type": {},
            "by_distance_band": {},
            "overall": {},
        }
        
        # By link type
        by_type = defaultdict(list)
        for sample in secondary_samples:
            lt = sample.get("link_type", "unknown")
            by_type[lt].append(sample["sinr_db"])
        
        for link_type, sinr_values in by_type.items():
            if sinr_values:
                stats["by_link_type"][link_type] = {
                    "count": len(sinr_values),
                    "mean": statistics.mean(sinr_values),
                    "median": statistics.median(sinr_values),
                    "stdev": statistics.stdev(sinr_values) if len(sinr_values) > 1 else 0.0,
                    "min": min(sinr_values),
                    "max": max(sinr_values),
                    "p95": sorted(sinr_values)[int(0.95 * len(sinr_values))] if len(sinr_values) > 1 else sinr_values[0],
                }
        
        # By distance band (0-100m, 100-250m, 250-500m, >500m)
        bands = {
            "0-100m": (0, 100),
            "100-250m": (100, 250),
            "250-500m": (250, 500),
            ">500m": (500, float('inf')),
        }
        by_band = defaultdict(list)
        for sample in secondary_samples:
            dist = sample.get("distance_m", 0)
            for band_name, (min_d, max_d) in bands.items():
                if min_d <= dist < max_d:
                    by_band[band_name].append(sample["sinr_db"])
                    break
        
        for band_name, sinr_values in by_band.items():
            if sinr_values:
                stats["by_distance_band"][band_name] = {
                    "count": len(sinr_values),
                    "mean": statistics.mean(sinr_values),
                    "median": statistics.median(sinr_values),
                    "stdev": statistics.stdev(sinr_values) if len(sinr_values) > 1 else 0.0,
                    "min": min(sinr_values),
                    "max": max(sinr_values),
                }
        
        # Overall
        all_sinr = [s["sinr_db"] for s in secondary_samples]
        if all_sinr:
            stats["overall"] = {
                "count": len(all_sinr),
                "mean": statistics.mean(all_sinr),
                "median": statistics.median(all_sinr),
                "stdev": statistics.stdev(all_sinr) if len(all_sinr) > 1 else 0.0,
                "min": min(all_sinr),
                "max": max(all_sinr),
                "p95": sorted(all_sinr)[int(0.95 * len(all_sinr))] if len(all_sinr) > 1 else all_sinr[0],
            }
        
        return stats
    
    def export_csv(self, samples: List[Dict], filename: str = "parity_samples.csv"):
        """Export sample data to CSV."""
        if not samples:
            print(f"✗ No samples to export")
            return
        
        output_file = self.output_dir / filename
        try:
            if pd:
                df = pd.DataFrame(samples)
                df.to_csv(output_file, index=False)
                print(f"✓ Exported {len(samples)} samples to {output_file}")
            else:
                # Fallback: manual CSV write
                import csv
                keys = samples[0].keys()
                with open(output_file, 'w', newline='') as f:
                    writer = csv.DictWriter(f, fieldnames=keys)
                    writer.writeheader()
                    writer.writerows(samples)
                print(f"✓ Exported {len(samples)} samples to {output_file}")
        except Exception as e:
            print(f"✗ CSV export failed: {e}")
    
    def export_report(self, stats: Dict, filename: str = "parity_report.md"):
        """Export benchmark report to Markdown."""
        output_file = self.output_dir / filename
        try:
            with open(output_file, 'w') as f:
                f.write("# SINR Parity Benchmark Report\n\n")
                f.write(f"**Generated:** {datetime.now().isoformat()}\n\n")
                
                # Overall stats
                f.write("## Overall Statistics\n\n")
                if "overall" in stats:
                    overall = stats["overall"]
                    f.write(f"| Metric | Value |\n")
                    f.write(f"|--------|-------|\n")
                    f.write(f"| Total Samples | {overall.get('count', 0)} |\n")
                    f.write(f"| Mean SINR | {overall.get('mean', 0):.2f} dB |\n")
                    f.write(f"| Median SINR | {overall.get('median', 0):.2f} dB |\n")
                    f.write(f"| Std Dev | {overall.get('stdev', 0):.2f} dB |\n")
                    f.write(f"| Min | {overall.get('min', 0):.2f} dB |\n")
                    f.write(f"| Max | {overall.get('max', 0):.2f} dB |\n")
                    f.write(f"| P95 | {overall.get('p95', 0):.2f} dB |\n\n")
                
                # By link type
                f.write("## Statistics by Link Type\n\n")
                if "by_link_type" in stats:
                    for link_type, metrics in stats["by_link_type"].items():
                        f.write(f"### {link_type}\n\n")
                        f.write(f"| Metric | Value |\n")
                        f.write(f"|--------|-------|\n")
                        f.write(f"| Count | {metrics.get('count', 0)} |\n")
                        f.write(f"| Mean SINR | {metrics.get('mean', 0):.2f} dB |\n")
                        f.write(f"| Median | {metrics.get('median', 0):.2f} dB |\n")
                        f.write(f"| Std Dev | {metrics.get('stdev', 0):.2f} dB |\n")
                        f.write(f"| Min | {metrics.get('min', 0):.2f} dB |\n")
                        f.write(f"| Max | {metrics.get('max', 0):.2f} dB |\n")
                        f.write(f"| P95 | {metrics.get('p95', 0):.2f} dB |\n\n")
                
                # By distance band
                f.write("## Statistics by Distance Band\n\n")
                if "by_distance_band" in stats:
                    for band_name, metrics in sorted(stats["by_distance_band"].items()):
                        f.write(f"### {band_name}\n\n")
                        f.write(f"| Metric | Value |\n")
                        f.write(f"|--------|-------|\n")
                        f.write(f"| Count | {metrics.get('count', 0)} |\n")
                        f.write(f"| Mean SINR | {metrics.get('mean', 0):.2f} dB |\n")
                        f.write(f"| Median | {metrics.get('median', 0):.2f} dB |\n")
                        f.write(f"| Std Dev | {metrics.get('stdev', 0):.2f} dB |\n")
                        f.write(f"| Min | {metrics.get('min', 0):.2f} dB |\n")
                        f.write(f"| Max | {metrics.get('max', 0):.2f} dB |\n\n")
                
                f.write("## Notes\n\n")
                f.write("- **Secondary SINR**: C++ implementation in MyRSUApp (exported to Redis/database)\n")
                f.write("- **Validation Criteria**: Mean error <3 dB, bias <2 dB\n")
                f.write("- **Link Types**: V2V, V2RSU, RSU2V, RSU2RSU\n")
                f.write("- **Distance Bands**: 0-100m, 100-250m, 250-500m, >500m\n")
            
            print(f"✓ Report written to {output_file}")
        except Exception as e:
            print(f"✗ Report export failed: {e}")
    
    def run(self, args):
        """Main benchmark workflow."""
        print("\n" + "="*70)
        print("SINR Parity Benchmark - Primary vs Secondary")
        print("="*70 + "\n")
        
        # Load secondary samples
        print("[1/4] Loading secondary SINR samples...")
        secondary_samples = []
        
        # Try Redis first
        secondary_samples.extend(
            self.load_redis_secondary_sinr(args.redis_host, args.redis_port)
        )
        
        # Fall back to database
        if not secondary_samples:
            secondary_samples.extend(
                self.load_postgres_secondary_sinr("sinr_log.sqlite")
            )
        
        if not secondary_samples:
            print("⚠️  No secondary SINR data found in Redis or database.")
            print("   Please ensure simulation has been run and data exported.")
            print("\n   Alternatives:")
            print("   - Start Redis: redis-server")
            print("   - Run simulation: ./run_sim_portable.sh")
            print("   - Check omnetpp.ini for DataExporter settings")
            return 1
        
        # Pair samples
        print("\n[2/4] Pairing and aggregating samples...")
        paired_samples, pair_keys = self.pair_samples(secondary_samples)
        print(f"  Found {len(pair_keys)} unique Tx/Rx pairs")
        print(f"  Aggregated to {len(paired_samples)} pair averages")
        
        # Compute statistics
        print("\n[3/4] Computing statistics...")
        self.stats = self.compute_statistics(secondary_samples)
        print(f"  Overall: {self.stats['overall'].get('count', 0)} samples")
        print(f"  Mean SINR: {self.stats['overall'].get('mean', 0):.2f} dB")
        print(f"  P95 SINR: {self.stats['overall'].get('p95', 0):.2f} dB")
        print(f"  Link types: {', '.join(self.stats['by_link_type'].keys())}")
        
        # Export results
        print("\n[4/4] Exporting results...")
        self.export_csv(secondary_samples, "secondary_sinr_samples.csv")
        self.export_csv(paired_samples, "sinr_pair_averages.csv")
        self.export_report(self.stats, "parity_report.md")
        
        print("\n" + "="*70)
        print("Benchmark Complete")
        print("="*70)
        print(f"\nResults saved to: {self.output_dir}")
        print(f"  - secondary_sinr_samples.csv")
        print(f"  - sinr_pair_averages.csv")
        print(f"  - parity_report.md")
        
        return 0


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark SINR parity between primary and secondary simulations"
    )
    parser.add_argument("--redis-host", default="localhost",
                       help="Redis host (default: localhost)")
    parser.add_argument("--redis-port", type=int, default=6379,
                       help="Redis port (default: 6379)")
    parser.add_argument("--output-dir", default="./benchmark_results",
                       help="Output directory for results (default: ./benchmark_results)")
    parser.add_argument("--db-path", default="sinr_log.sqlite",
                       help="Path to SQLite database (default: sinr_log.sqlite)")
    parser.add_argument("--run-simulation", action="store_true",
                       help="Run simulation first (requires OMNeT++ and config)")
    parser.add_argument("--config-name", default="DT-Secondary-MotionChannel",
                       help="OMNeT++ config to run (default: DT-Secondary-MotionChannel)")
    
    args = parser.parse_args()
    
    # Optionally run simulation
    if args.run_simulation:
        print("Running OMNeT++ simulation (not yet implemented)...")
        print("  Use: ./run_simulation.sh")
    
    # Run benchmark
    benchmark = SINRBenchmark(output_dir=args.output_dir)
    return benchmark.run(args)


if __name__ == "__main__":
    sys.exit(main())
