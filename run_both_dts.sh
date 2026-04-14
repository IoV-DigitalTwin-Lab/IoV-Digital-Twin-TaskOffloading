#!/usr/bin/env bash
# Run both primary and secondary Digital Twins:
# - Secondary DT starts first in terminal (Cmdenv)
# - After 10 seconds, primary DT starts in UI (Qtenv)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Set OMNeT++ home directory explicitly
# The installation is at /opt/omnet/omnetpp-6.1 (note: "omnet" not "omnetpp")
export OMNETPP_HOME="${OMNETPP_HOME:-/opt/omnet/omnetpp-6.1}"
export VEINS_PATH="${VEINS_PATH:-../veins}"
export INET_PATH="${INET_PATH:-../inet4.5}"

if [ ! -f "$OMNETPP_HOME/setenv" ]; then
  echo "Error: OMNeT++ not found at $OMNETPP_HOME"
  echo "Please set OMNETPP_HOME to your OMNeT++ installation directory:"
  echo "  export OMNETPP_HOME=/path/to/omnetpp-6.1"
  exit 1
fi

if [ ! -d "$VEINS_PATH/src/veins" ]; then
  echo "Error: Veins not found at $VEINS_PATH"
  echo "Please set VEINS_PATH to your Veins installation directory:"
  echo "  export VEINS_PATH=/path/to/veins"
  exit 1
fi

echo "=========================================="
echo "IoV Digital Twin - Dual Simulation Launcher"
echo "=========================================="
echo "OMNeT++ Home: $OMNETPP_HOME"
echo "Veins Path:  $VEINS_PATH"
echo "INET Path:   $INET_PATH"
echo "DISPLAY:     ${DISPLAY:-NOT SET}"
echo ""

# Source OMNeT++ environment (temporarily disable -u for setenv compatibility)
set +u
source "$OMNETPP_HOME/setenv"
set -u

# Set X11 display mode for remote/forwarded displays
export QT_QPA_PLATFORM=xcb
export QT_X11_GL_SETTINGS=1

# Note: The secondary DT runs with offloading disabled, capturing mobility & channel data only
# The primary DT runs the full task offloading simulation with UI visualization

# Start secondary DT in background (terminal mode)
echo "[1/2] Starting Secondary DT (Motion + Channel Only) in background..."
echo "      Config: DT-Secondary-MotionChannel"
echo "      Output: results/DT-Secondary-MotionChannel-*.sca/.vec"
echo ""

./run_secondary_dt.sh "$@" &
SECONDARY_PID=$!
echo "      PID: $SECONDARY_PID (running in background)"
echo ""

# Wait 10 seconds
echo "[*] Waiting 10 seconds before launching Primary DT UI..."
sleep 10

echo ""
echo "[2/2] Starting Primary DT (Full Task Offloading) with UI..."
echo "      Config: DT-PreSync"
echo "      Output: results/DT-PreSync-*.sca/.vec"
echo ""

# Start primary DT in UI mode (Qtenv)
# The UI will display both vehicle and RSU activity, channel state, task queue, etc.
./run_sim_portable.sh -u Qtenv -c DT-PreSync -r 0 --qtenv-default-run=1 --*.manager.port=9999 "$@"

# Note: When you close the UI, the primary DT process will exit.
# The secondary DT will continue running in the background.
# To stop the secondary DT, use: kill $SECONDARY_PID or pkill -f 'DT-Secondary-MotionChannel'

wait $SECONDARY_PID 2>/dev/null || true
echo ""
echo "=========================================="
echo "Both simulations completed"
echo "=========================================="
