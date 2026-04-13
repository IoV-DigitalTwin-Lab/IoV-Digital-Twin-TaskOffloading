#!/bin/bash
# Quick benchmark workflow for SINR parity testing

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/benchmark_results"
REDIS_HOST="${REDIS_HOST:-localhost}"
REDIS_PORT="${REDIS_PORT:-6379}"

echo "=========================================="
echo "SINR Parity Benchmark Workflow"
echo "=========================================="
echo ""

# Check Redis
echo "[1/3] Checking Redis availability..."
if timeout 2 redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" ping >/dev/null 2>&1; then
    echo "  ✓ Redis available at $REDIS_HOST:$REDIS_PORT"
else
    echo "  ⚠ Redis not running. Starting redis-server..."
    if command -v redis-server &> /dev/null; then
        redis-server --daemonize yes --port "$REDIS_PORT"
        sleep 1
        echo "  ✓ Redis started"
    else
        echo "  ✗ redis-server not found. Install Redis or export data to database."
    fi
fi

# Run the Python benchmark script
echo ""
echo "[2/3] Running SINR benchmark analysis..."
cd "$SCRIPT_DIR"

# Use the Python environment from OMNeT++ if available
PYTHON_CMD="python3"
if [ -f "${OMNETPP_ROOT}/.venv/bin/python" ]; then
    PYTHON_CMD="${OMNETPP_ROOT}/.venv/bin/python"
fi

"$PYTHON_CMD" benchmark_sinr_parity.py \
    --redis-host "$REDIS_HOST" \
    --redis-port "$REDIS_PORT" \
    --output-dir "$OUTPUT_DIR"

# Display results summary
echo ""
echo "[3/3] Results Summary"
echo "=================="

if [ -f "$OUTPUT_DIR/parity_report.md" ]; then
    echo ""
    head -50 "$OUTPUT_DIR/parity_report.md"
    echo ""
    echo "Full report: $OUTPUT_DIR/parity_report.md"
fi

echo ""
echo "=========================================="
echo "Benchmark Complete"
echo "=========================================="
echo "Output saved to: $OUTPUT_DIR/"
echo ""
