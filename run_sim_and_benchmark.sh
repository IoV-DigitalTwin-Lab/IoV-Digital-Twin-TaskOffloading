#!/bin/bash
# Integrated workflow: Run simulation, wait for data export, then run benchmark

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM_TIMEOUT_SECONDS="${SIM_TIMEOUT_SECONDS:-120}"  # Run sim for 2 minutes by default
WAIT_FOR_DATA="${WAIT_FOR_DATA:-5}"  # Wait 5 seconds after sim for data export

echo "=========================================="
echo "Simulation + SINR Parity Benchmark Workflow"
echo "=========================================="
echo ""

# Check Redis
echo "[1/3] Checking Redis..."
if ! timeout 2 redis-cli ping >/dev/null 2>&1; then
    echo "  Starting Redis server..."
    redis-server --daemonize yes --port 6379
    sleep 1
fi
echo "  ✓ Redis ready"
echo ""

# Run simulation for specified duration
echo "[2/3] Running OMNeT++ simulation for ${SIM_TIMEOUT_SECONDS}s..."
cd "$SCRIPT_DIR"

# Run simulation in background with timeout
timeout "$SIM_TIMEOUT_SECONDS" ./run_sim_portable.sh -c DT-Secondary-MotionChannel 2>&1 | tee sim_run.log &
SIM_PID=$!

# Wait for completion or timeout
wait "$SIM_PID" 2>/dev/null || true

echo "  ✓ Simulation completed (or timed out after ${SIM_TIMEOUT_SECONDS}s)"
echo ""

# Wait for data export to finish
echo "  Waiting ${WAIT_FOR_DATA}s for data export to complete..."
sleep "$WAIT_FOR_DATA"
echo ""

# Check what data we have
echo "[2/3b] Checking available data..."
REDIS_KEYS=$(redis-cli keys "sinr:*" 2>/dev/null | wc -l || true)
echo "  Redis SINR samples: $REDIS_KEYS"

if [ -f "sinr_log.sqlite" ]; then
    SQLITE_ROWS=$(sqlite3 sinr_log.sqlite "SELECT COUNT(*) FROM sinr_log 2>/dev/null;" 2>/dev/null || echo "0")
    echo "  SQLite SINR rows: $SQLITE_ROWS"
fi
echo ""

# Run benchmark
echo "[3/3] Running SINR parity benchmark..."
./run_benchmark.sh

echo ""
echo "=========================================="
echo "Workflow Complete!"
echo "=========================================="
echo ""
echo "Outputs:"
echo "  - Simulation log: sim_run.log"
echo "  - Benchmark results: benchmark_results/"
echo ""
