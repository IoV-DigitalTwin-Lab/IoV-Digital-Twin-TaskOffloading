#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Keep launcher display and controller runtime default aligned (100 ms).
export DT2_POLL_INTERVAL_S="${DT2_POLL_INTERVAL_S:-0.1}"

# Locate Python (prefer .venv alongside OMNeT++ tree).
PYTHON="${PYTHON:-python}"
for candidate in \
    "$(dirname "$SCRIPT_DIR")/../.venv/bin/python" \
    "/opt/omnet/omnetpp-6.1/.venv/bin/python" \
    python3 python; do
    if command -v "$candidate" &>/dev/null 2>&1; then
        PYTHON="$candidate"
        break
    fi
done

CTRL_PID=""
cleanup() {
    if [[ -n "$CTRL_PID" ]] && kill -0 "$CTRL_PID" 2>/dev/null; then
        echo "[run_secondary_dt] Stopping external controller (PID $CTRL_PID)..."
        kill "$CTRL_PID" 2>/dev/null || true
        wait "$CTRL_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Start the external controller in the background.
# It publishes dt2:pred:* predictions that the secondary simulation reads
# each Q-cycle to compute SINR values and write dt2:q:*:entries.
echo "[run_secondary_dt] Starting external controller (poll interval: ${DT2_POLL_INTERVAL_S}s)..."
"$PYTHON" "$SCRIPT_DIR/external_controller.py" &
CTRL_PID=$!
echo "[run_secondary_dt] External controller PID=$CTRL_PID"

# Give the controller one poll cycle to publish initial predictions before
# the simulation starts issuing Q-cycles.
sleep "${DT2_CTRL_WARMUP_S:-0.1}"

# Run the secondary digital-twin simulation.
# Extra args are forwarded to run_sim_portable.sh.
echo "[run_secondary_dt] Starting secondary simulation..."
./run_sim_portable.sh -u Cmdenv -c DT-Secondary-MotionChannel -r 0 --*.manager.port=10000 "$@"
