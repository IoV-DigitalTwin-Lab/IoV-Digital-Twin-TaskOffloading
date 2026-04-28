#!/usr/bin/env bash
# Run both primary and secondary Digital Twins:
# - Secondary DT starts first and keeps the terminal focused on live logs
# - After 10 seconds, primary DT starts detached with its output logged to file

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RESULTS_DIR="$SCRIPT_DIR/results"
PRIMARY_LOG="$RESULTS_DIR/primary_dt.log"
SECONDARY_LOG="$RESULTS_DIR/secondary_dt.log"
mkdir -p "$RESULTS_DIR"

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

PRIMARY_PID=""
SECONDARY_PID=""

cleanup() {
  if [ -n "$PRIMARY_PID" ] && kill -0 "$PRIMARY_PID" 2>/dev/null; then
    kill -TERM "$PRIMARY_PID" 2>/dev/null || true
    wait "$PRIMARY_PID" 2>/dev/null || true
  fi

  if [ -n "$SECONDARY_PID" ] && kill -0 "$SECONDARY_PID" 2>/dev/null; then
    kill -TERM "$SECONDARY_PID" 2>/dev/null || true
    wait "$SECONDARY_PID" 2>/dev/null || true
  fi
}

on_signal() {
  cleanup
  exit 130
}

trap cleanup EXIT
trap on_signal INT TERM

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

./run_secondary_dt.sh "$@" > "$SECONDARY_LOG" 2>&1 &
SECONDARY_PID=$!
echo "      PID: $SECONDARY_PID (running in background)"
echo ""

# Wait 10 seconds
echo "[*] Waiting 10 seconds before launching Primary DT UI..."
sleep 10

echo ""
echo "[2/2] Starting Primary DT (Full Task Offloading) in background..."
echo "      Config: DT-PreSync"
echo "      Output: results/DT-PreSync-*.sca/.vec"
echo ""

echo "      Primary DT log: $PRIMARY_LOG"

# Start primary DT detached so this terminal stays focused on secondary logs.
# The launcher runs headless-safe because run_sim_portable.sh will select Cmdenv.
CMDENV_MODE=1 DT2_AUTO_CONTROLLER=1 ./run_sim_portable.sh -c DT-PreSync -r 0 --*.manager.port=9999 "$@" > "$PRIMARY_LOG" 2>&1 &
PRIMARY_PID=$!
echo "      PID: $PRIMARY_PID (running in background)"
echo ""

# The secondary DT will continue running in the foreground of this terminal.
# To stop either simulation, press Ctrl+C here.

wait $SECONDARY_PID 2>/dev/null || true
echo ""
echo "=========================================="
echo "Both simulations completed"
echo "=========================================="
