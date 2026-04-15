#!/usr/bin/env python3
"""
Mock SINR Data Generator for Benchmark Testing

Generates synthetic secondary SINR data and stores in Redis.
Useful for testing the benchmark pipeline without running full SUMO+OMNeT++ simulation.

Usage:
    python3 mock_sinr_generator.py [--redis-host localhost] [--redis-port 6379] \
                                   [--num-pairs 50] [--samples-per-pair 10]
"""

import json
import math
import random
import argparse
import sys
from typing import List, Dict, Tuple

try:
    import redis
except ImportError:
    redis = None


class MockSINRGenerator:
    """Generate realistic mock SINR data for benchmark testing."""
    
    def __init__(self, redis_host: str = "localhost", redis_port: int = 6379):
        self.redis_host = redis_host
        self.redis_port = redis_port
        self.redis = None
        self.tx_nodes = ["rsu[0]", "rsu[1]", "rsu[2]"]
        self.rx_nodes = ["veh[0]", "veh[1]", "veh[2]", "veh[3]", "veh[4]"]
        self.network_bounds = (0, 2000, 0, 2000)  # (x_min, x_max, y_min, y_max)
        
    def connect_redis(self) -> bool:
        """Connect to Redis."""
        if redis is None:
            print("✗ redis-py not installed. Install with: pip install redis")
            return False
        
        try:
            self.redis = redis.Redis(host=self.redis_host, port=self.redis_port, 
                                     decode_responses=True)
            self.redis.ping()
            print(f"✓ Connected to Redis at {self.redis_host}:{self.redis_port}")
            return True
        except Exception as e:
            print(f"✗ Redis connection failed: {e}")
            print("  Make sure Redis is running: redis-server --daemonize yes")
            return False
    
    def generate_position(self) -> Tuple[float, float]:
        """Generate random position within network bounds."""
        x_min, x_max, y_min, y_max = self.network_bounds
        x = random.uniform(x_min, x_max)
        y = random.uniform(y_min, y_max)
        return x, y
    
    def estimate_path_loss(self, distance_m: float, alpha: float = 2.2) -> float:
        """Estimate path loss with desired exponent.
        
        Formula: PL = 20*log10(4π/λ) + 10*α*log10(d)
        At 5.89 GHz: 20*log10(4π/λ) ≈ 107.6 dB
        """
        if distance_m < 1.0:
            distance_m = 1.0
        
        ref_loss_db = 107.6  # At 1m, 5.89 GHz
        path_loss_db = ref_loss_db + 10.0 * alpha * math.log10(distance_m)
        return path_loss_db
    
    def estimate_shadowing(self, distance_m: float, sigma_db: float = 6.0) -> float:
        """Estimate lognormal shadowing with distance-dependent correlation.
        
        Shadowing characterized by sigma=6 dB with spatial correlation ~50m.
        Return small variations per sample (high correlation).
        """
        # Spatially correlated component (slow variation)
        slow_component = random.gauss(0, sigma_db * 0.7)
        # Fast uncorrelated component
        fast_component = random.gauss(0, sigma_db * 0.3)
        return slow_component + fast_component
    
    def estimate_nakagami_fading(self) -> float:
        """Estimate Nakagami fading (m=1, Rayleigh).
        
        For m=1, fading power has exponential distribution.
        Converts to dB: 10*log10(power), typically -3 to +3 dB.
        """
        # Exponential RV with mean 1 (Nakagami m=1)
        power_linear = random.expovariate(1.0)
        power_db = 10.0 * math.log10(max(1e-12, power_linear))
        # Clamp to realistic range
        return max(-30.0, min(30.0, power_db))
    
    def estimate_vehicle_shadowing(self, distance_m: float) -> float:
        """Estimate vehicle shadowing from knife-edge diffraction.
        
        Simple approximation: ~0-3 dB per vehicle, max ~10 dB.
        Distance-dependent: more vehicles in near field.
        """
        num_vehicles = int(max(0, (distance_m / 100.0) * random.gauss(0.5, 0.3)))
        num_vehicles = min(3, num_vehicles)  # Cap at 3 vehicles
        loss_per_vehicle = random.uniform(1.5, 3.0)
        return num_vehicles * loss_per_vehicle
    
    def compute_sinr_db(self, distance_m: float, link_type: str = "V2RSU") -> float:
        """Compute realistic SINR for given distance and link type.
        
        SINR = TX_power - path_loss - shadowing_dist - shadowing_vehicle - fading + noise_fig
        """
        tx_power_dbm = 33.0  # 33 dBm (2000 mW)
        noise_floor_dbm = -110.0  # Receiver noise floor
        noise_figure_db = 8.0  # Receiver NF
        effective_noise_dbm = noise_floor_dbm + noise_figure_db
        
        # Path loss (distance-dependent)
        path_loss_db = self.estimate_path_loss(distance_m, alpha=2.2)
        
        # Lognormal shadowing (static objects, sigma=6dB, correlation=50m)
        shadow_db = self.estimate_shadowing(distance_m, sigma_db=6.0)
        
        # Vehicle shadowing (dynamic, geometry-based)
        vehicle_shadow_db = self.estimate_vehicle_shadowing(distance_m)
        
        # Fading (Nakagami m=1 Rayleigh)
        fading_db = self.estimate_nakagami_fading()
        
        # Compute RX power
        rx_power_dbm = tx_power_dbm - path_loss_db - shadow_db - vehicle_shadow_db + fading_db
        
        # SINR (assuming interference power ~-100 dBm for typical network)
        interference_power_dbm = -100.0
        sinr_db = rx_power_dbm - interference_power_dbm
        
        # Link-type-specific adjustments
        if link_type == "RSU2RSU":
            sinr_db += 2.0  # Higher SNR for line-of-sight
        elif link_type == "V2V":
            sinr_db -= 1.0  # Lower due to obstruction
        
        return sinr_db
    
    def generate_samples(self, num_pairs: int = 50, samples_per_pair: int = 10) -> List[Dict]:
        """Generate mock SINR samples.
        
        Returns: List of {tx_id, rx_id, tx_x, tx_y, rx_x, rx_y, distance_m, sinr_db, link_type}
        """
        samples = []
        
        # Generate Tx/Rx pairs
        pairs = []
        for tx_node in self.tx_nodes:
            for rx_node in self.rx_nodes:
                if tx_node != rx_node:  # Can't transmit to self
                    pairs.append((tx_node, rx_node))
        
        # Random shuffle and limit to num_pairs
        random.shuffle(pairs)
        pairs = pairs[:num_pairs]
        
        print(f"Generating {len(pairs)} Tx/Rx pairs × {samples_per_pair} samples...")
        
        for tx_id, rx_id in pairs:
            # Determine link type
            if tx_id.startswith("rsu") and rx_id.startswith("rsu"):
                link_type = "RSU2RSU"
            elif tx_id.startswith("rsu") and rx_id.startswith("veh"):
                link_type = "RSU2V"
            elif tx_id.startswith("veh") and rx_id.startswith("rsu"):
                link_type = "V2RSU"
            else:
                link_type = "V2V"
            
            # Generate samples for this pair
            for sample_idx in range(samples_per_pair):
                tx_x, tx_y = self.generate_position()
                rx_x, rx_y = self.generate_position()
                distance_m = math.sqrt((tx_x - rx_x)**2 + (tx_y - rx_y)**2)
                sinr_db = self.compute_sinr_db(distance_m, link_type)
                
                samples.append({
                    "tx_id": tx_id,
                    "rx_id": rx_id,
                    "tx_x": round(tx_x, 2),
                    "tx_y": round(tx_y, 2),
                    "rx_x": round(rx_x, 2),
                        "rx_y": round(rx_y, 2),  # Fixed typo
                    "distance_m": round(distance_m, 2),
                    "sinr_db": round(sinr_db, 2),
                    "link_type": link_type,
                    "source": "mock_generator",
                })
        
        print(f"  Generated {len(samples)} total samples")
        return samples
    
    def store_in_redis(self, samples: List[Dict]):
        """Store samples in Redis with sinr:* keys."""
        if not self.redis:
            print("✗ Redis not connected")
            return False
        
        try:
            for idx, sample in enumerate(samples):
                key = f"sinr:{sample['tx_id']}:{sample['rx_id']}:{idx}"
                data = json.dumps(sample)
                self.redis.set(key, data)
            
            # Also set a metadata key
            metadata = {
                "generator": "mock_sinr_generator",
                "total_samples": len(samples),
                "num_pairs": len(set((s["tx_id"], s["rx_id"]) for s in samples)),
            }
            self.redis.set("sinr:metadata", json.dumps(metadata))
            
            print(f"✓ Stored {len(samples)} samples in Redis")
            
            # Verify
            keys = self.redis.keys("sinr:*")
            print(f"  Keys in Redis: {len(keys)}")
            return True
        except Exception as e:
            print(f"✗ Redis storage failed: {e}")
            return False
    
    def run(self, num_pairs: int = 50, samples_per_pair: int = 10):
        """Main workflow."""
        print("\n" + "="*70)
        print("Mock SINR Data Generator")
        print("="*70 + "\n")
        
        # Connect
        if not self.connect_redis():
            return 1
        
        print("")
        
        # Generate samples
        samples = self.generate_samples(num_pairs, samples_per_pair)
        
        print("")
        
        # Store in Redis
        if not self.store_in_redis(samples):
            return 1
        
        print("\n" + "="*70)
        print("Mock Data Generation Complete")
        print("="*70 + "\n")
        print(f"Ready to run benchmark:")
        print(f"  ./run_benchmark.sh\n")
        
        return 0


def main():
    parser = argparse.ArgumentParser(
        description="Generate mock SINR data for benchmark testing"
    )
    parser.add_argument("--redis-host", default="localhost",
                       help="Redis host (default: localhost)")
    parser.add_argument("--redis-port", type=int, default=6379,
                       help="Redis port (default: 6379)")
    parser.add_argument("--num-pairs", type=int, default=50,
                       help="Number of Tx/Rx pairs (default: 50)")
    parser.add_argument("--samples-per-pair", type=int, default=10,
                       help="Samples per pair (default: 10)")
    
    args = parser.parse_args()
    
    generator = MockSINRGenerator(args.redis_host, args.redis_port)
    return generator.run(args.num_pairs, args.samples_per_pair)


if __name__ == "__main__":
    sys.exit(main())
