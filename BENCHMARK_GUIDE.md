# SINR Parity Benchmark Guide

## Overview

The benchmark suite compares SINR values from the **primary simulation** (Veins OMNeT++) with the **secondary estimator** (C++ implementation in MyRSUApp). The goal is to quantify how closely they match.

## Quick Start

### Option 1: Automated Workflow (Bash)
```bash
# Make executable
chmod +x run_benchmark.sh

# Run with defaults
./run_benchmark.sh

# Or with custom Redis connection
REDIS_HOST=192.168.1.10 REDIS_PORT=6379 ./run_benchmark.sh
```

### Option 2: Direct Python
```bash
python3 benchmark_sinr_parity.py \
    --redis-host localhost \
    --redis-port 6379 \
    --output-dir ./benchmark_results
```

## Prerequisites

### Requirements
- **Python 3.7+**
- **Redis** (for secondary SINR export from simulation)
  - Or: SQLite database with recorded SINR values

### Optional Dependencies
```bash
# For CSV export with pandas (faster)
pip install redis pandas

# Or use pure Python variants (slower but no deps)
pip install redis
```

## Data Flow

### 1. Generate Secondary SINR Data

Run the OMNeT++ simulation to generate secondary SINR values:

```bash
# Option A: GUI simulation
./run_simulation.sh

# Option B: Batch simulation (faster, no display)
opp_run -c DT-Secondary-MotionChannel -u Cmdenv
```

The simulation will:
- Compute SINR at RSU via secondary estimator
- Export to Redis (if configured in omnetpp.ini)
- Also log to database (sinr_log.sqlite)

### 2. Run Benchmark Analysis

```bash
./run_benchmark.sh
```

This will:
1. Query Redis for secondary SINR values
2. Aggregate by (tx_id, rx_id) pairs
3. Compute statistics by link type (V2V, V2RSU, RSU2V, RSU2RSU)
4. Compute statistics by distance band (0-100m, 100-250m, 250-500m, >500m)
5. Export CSV + Markdown report

### 3. Review Results

```bash
# View summary
cat benchmark_results/parity_report.md

# Detailed samples per pair
cat benchmark_results/sinr_pair_averages.csv

# All samples
cat benchmark_results/secondary_sinr_samples.csv
```

## Output Files

### `parity_report.md`
Human-readable markdown report with statistics tables:
- **Overall**: Total samples, mean, median, std dev, min/max, P95
- **By Link Type**: Separate stats for V2V, V2RSU, RSU2V, RSU2RSU
- **By Distance Band**: Stats binned by transmission distance

### `secondary_sinr_samples.csv`
Raw secondary SINR samples (one row per time sample):
```csv
tx_id,rx_id,tx_x,tx_y,rx_x,rx_y,sinr_db,distance_m,link_type,sample_count,source
rsu[0],veh[1],100.5,200.3,105.1,198.7,-5.23,8.5,V2RSU,1,secondary
veh[2],rsu[0],110.0,210.0,100.5,200.3,8.15,18.2,V2RSU,1,secondary
```

### `sinr_pair_averages.csv`
Aggregated samples by (tx_id, rx_id) pair:
```csv
tx_id,rx_id,distance_m,sinr_db,link_type,sample_count
rsu[0],veh[1],8.5,-5.20,V2RSU,42
veh[2],rsu[0],18.2,8.18,V2RSU,35
```

## Validation Criteria

**Success threshold** (based on phase goals):
- **Mean absolute error**: < 3 dB (acceptable; primary/secondary model differences)
- **Bias by link type**: < 2 dB (systematic shift)
- **P95 SINR error**: < 5 dB (worst-case samples)

**Current Implementation Status:**
- ✓ TX power: 33 dBm (aligned)
- ✓ Path loss: α=2.2 (aligned)
- ✓ Vehicle shadowing: Knife-edge diffraction (implemented)
- ✓ LogNormal fading: σ=6 dB, correlation=50 m (aligned)
- ✓ Nakagami: m=1.0 Rayleigh fading (implemented, parameterized)

## Customization

### Changing Distance Bands
Edit `benchmark_sinr_parity.py`, function `compute_statistics()`:
```python
bands = {
    "0-50m": (0, 50),           # Change me
    "50-150m": (50, 150),       # Change me
    "150-300m": (150, 300),     # Change me
    ">300m": (300, float('inf')),
}
```

### Changing Sample Limits
Edit `load_redis_secondary_sinr()`:
```python
for key in keys[:10000]:  # Change max samples here
```

### Adding Primary SINR Comparison
Once primary (Veins) SINR is logged, extend script:
```python
primary_samples = self.load_omnetpp_vectors("results/Primary.vec")
# Compute error metrics per (tx_id, rx_id) pair
errors = [primary_sinr - secondary_sinr for ...]
```

## Troubleshooting

### No data in Redis
1. Ensure Redis is running: `redis-cli ping` → should return `PONG`
2. Check omnetpp.ini has DataExporter module enabled:
   ```ini
   *.rsu[*].appl.redisExportEnabled = true
   ```
3. Run simulation and wait for cycle completion:
   ```bash
   ./run_sim_portable.sh
   ```
4. Verify data in Redis:
   ```bash
   redis-cli keys "sinr:*" | head -10
   ```

### Redis connection refused
```bash
# Start Redis
redis-server --port 6379 --daemonize yes

# Or specify custom port
REDIS_PORT=6380 ./run_benchmark.sh
```

### Python module not found
```bash
# Install dependencies
pip3 install redis pandas

# Or use bundled Python from OMNeT++
/opt/omnet/omnetpp-6.1/.venv/bin/python3 benchmark_sinr_parity.py
```

### No output files created
Check `benchmark_results/` directory:
```bash
ls -la benchmark_results/
```

If empty, ensure script ran to completion:
```bash
python3 benchmark_sinr_parity.py 2>&1 | tail -20
```

## Integration with CI/CD

### Automated Benchmark on Simulation Completion
```bash
#!/bin/bash
# run_sim_with_benchmark.sh

# Run simulation
./run_sim_portable.sh

# Wait for data export
sleep 5

# Run benchmark
./run_benchmark.sh

# Archive results
tar czf benchmark_results_$(date +%Y%m%d_%H%M%S).tar.gz benchmark_results/
```

### GitHub Actions Example
```yaml
- name: Run SINR Benchmark
  run: |
    ./run_benchmark.sh
    
- name: Upload Results
  uses: actions/upload-artifact@v2
  with:
    name: benchmark-results
    path: benchmark_results/
```

## Next Steps

1. **Run simulation** to generate secondary SINR data
2. **Run benchmark** to see current parity statistics
3. **Review report** for gap analysis
4. **If gaps exist**: Adjust parameters in omnetpp.ini (Nakagami m, path loss α, etc.) and retest
5. **Once aligned**: Lock in validated configuration

## Related Files

- [MyRSUApp.cc](MyRSUApp.cc) - Secondary SINR implementation
- [omnetpp.ini](omnetpp.ini) - Simulation config (including secondary parameters)
- [config.xml](config.xml) - Primary (Veins) physic models
- [SINR_CALCULATION_NOTES.md](SINR_CALCULATION_NOTES.md) - Design details

---

**Questions?** Check simulation logs:
```bash
tail -100 omnetpp.log
```
