#!/usr/bin/env bash
set -euo pipefail

# Runs the primary simulation and extracts the global slot activity factor summary.
# Usage:
#   ./run_activity_factor.sh [omnetpp args...]
# Example:
#   ./run_activity_factor.sh -u Cmdenv -c General --sim-time-limit=120s

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$ROOT_DIR/results"
mkdir -p "$LOG_DIR"

TS="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_DIR/activity_factor_${TS}.log"

cd "$ROOT_DIR"

./run_simulation.sh \
  -u Cmdenv \
  --cmdenv-express-mode=true \
  --cmdenv-status-frequency=30s \
  "$@" | tee "$LOG_FILE"

SUMMARY_LINE="$(grep -E "ACTIVITY_FACTOR_SUMMARY" "$LOG_FILE" | tail -n 1 || true)"
if [[ -z "$SUMMARY_LINE" ]]; then
  echo "ERROR: No ACTIVITY_FACTOR_SUMMARY line found in $LOG_FILE"
  echo "Make sure the simulation ran to completion and MyRSUApp logger is active."
  exit 1
fi

echo ""
echo "=== Activity Factor Result ==="
echo "$SUMMARY_LINE"
echo "log_file=$LOG_FILE"
