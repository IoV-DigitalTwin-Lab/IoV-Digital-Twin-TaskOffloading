#!/bin/bash
# Launch OMNeT++ simulation with Qtenv GUI
# This script sets up all required environment variables automatically

cd "$(dirname "$0")"

# Set display for X11 forwarding
if [ -z "$DISPLAY" ]; then
    # Try to get DISPLAY from IDE process
    IDE_DISPLAY=$(ps e -p $(pgrep -f omnetpp | head -1) 2>/dev/null | grep -o "DISPLAY=[^ ]*" | head -1 | cut -d= -f2)
    if [ -n "$IDE_DISPLAY" ]; then
        export DISPLAY=$IDE_DISPLAY
    else
        export DISPLAY=localhost:10.0
    fi
fi

# Set Qt platform
export QT_QPA_PLATFORM=xcb

# Set library path for hiredis and OMNeT++
export LD_LIBRARY_PATH=/home/mihiraja/.local/lib:/opt/omnet/omnetpp-6.1/lib:/opt/omnet/omnetpp-6.1/workspace/inet4.5/src:/opt/omnet/veins/src:$LD_LIBRARY_PATH

# Configuration (can be overridden by command line)
CONFIG="${1:-Phase4-UrbanEnvironment}"
RUN="${2:-0}"

echo "========================================="
echo "Launching OMNeT++ Qtenv GUI"
echo "========================================="
echo "Configuration: $CONFIG"
echo "Run: $RUN"
echo "Display: $DISPLAY"
echo "========================================="

# Launch simulation
./out/gcc-release/IoV-Digital-Twin-TaskOffloading \
    -u Qtenv \
    -c "$CONFIG" \
    -r "$RUN" \
    -n .:../inet4.5/src:/opt/omnet/veins/src/veins \
    "$@"
