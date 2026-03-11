#!/bin/bash
# Run simulation with correct library paths and NED paths

# Add local hiredis library to library path
export LD_LIBRARY_PATH=/home/mihiraja/.local/lib:$LD_LIBRARY_PATH

# Set display if not set (get from IDE process if available)
if [ -z "$DISPLAY" ]; then
    IDE_DISPLAY=$(ps e -p $(pgrep -f omnetpp | head -1) 2>/dev/null | grep -o "DISPLAY=[^ ]*" | head -1 | cut -d= -f2)
    if [ -n "$IDE_DISPLAY" ]; then
        export DISPLAY=$IDE_DISPLAY
    fi
fi

# Set Qt platform plugin for X11
export QT_QPA_PLATFORM=xcb

# Run the simulation  
exec ./out/gcc-release/IoV-Digital-Twin-TaskOffloading \
    -n .:../inet4.5/src:/opt/omnet/veins/src/veins \
    "$@"
