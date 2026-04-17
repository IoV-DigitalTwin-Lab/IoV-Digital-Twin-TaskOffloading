#!/usr/bin/env bash
# Diagnostic script to check display and run simulations with debug info

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

export OMNETPP_HOME="${OMNETPP_HOME:-/opt/omnet/omnetpp-6.1}"
export VEINS_PATH="${VEINS_PATH:-../veins}"
export INET_PATH="${INET_PATH:-../inet4.5}"

# Source OMNeT++ environment (temporarily disable -u for setenv compatibility)
set +u
source "$OMNETPP_HOME/setenv"
set -u

echo "=========================================="
echo "Display & Environment Diagnostic"
echo "=========================================="
echo "DISPLAY: ${DISPLAY:-NOT SET}"
echo "XAUTHORITY: ${XAUTHORITY:-NOT SET}"
echo "OMNeT++ Home: $OMNETPP_HOME"
echo "Veins Path:  $VEINS_PATH"
echo "INET Path:   $INET_PATH"
echo ""

# Check if X11 is available
if command -v xhost &> /dev/null; then
    echo "X11 Status:"
    xhost >& /dev/null && echo "  ✓ X11 is working" || echo "  ✗ X11 connection failed"
else
    echo "  ⚠ xhost not available (X11 tools not installed)"
fi

echo "=========================================="
echo ""

# Ask user which simulation to run
echo "Choose an option:"
echo "1) Run Primary DT with UI (Qtenv)"
echo "2) Run Secondary DT in terminal (Cmdenv)"
echo "3) Run Secondary DT in background, then Primary DT with UI"
echo ""
read -p "Enter choice (1-3): " choice

case $choice in
    1)
        echo "Starting Primary DT with UI..."
        export QT_QPA_PLATFORM=xcb
        ./run_sim_portable.sh -u Qtenv -c DT-PreSync -r 0 --qtenv-default-run=1 --*.manager.port=9999 "$@"
        ;;
    2)
        echo "Starting Secondary DT in terminal..."
        ./run_secondary_dt.sh "$@"
        ;;
    3)
        echo "Starting both simulations..."
        # Secondary in fully detached background process
        # (prevents terminal contention that can block Qtenv startup)
        nohup ./run_secondary_dt.sh "$@" > results/secondary_dt.log 2>&1 &
        SECONDARY_PID=$!
        echo "Secondary DT started with PID: $SECONDARY_PID"
        echo "Secondary log: results/secondary_dt.log"
        echo "Waiting 10 seconds..."
        sleep 10
        
        # Primary with UI
        echo "Starting Primary DT with UI..."
        export QT_QPA_PLATFORM=xcb
        ./run_sim_portable.sh -u Qtenv -c DT-PreSync -r 0 --qtenv-default-run=1 --*.manager.port=9999 "$@"
        
        wait $SECONDARY_PID 2>/dev/null || true
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac



# tail -f results/secondary_dt.log