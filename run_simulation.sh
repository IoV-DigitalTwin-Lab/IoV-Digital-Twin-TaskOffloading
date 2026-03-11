#!/bin/bash
# Run simulation with correct library paths and NED paths

# Add local hiredis library to library path
export LD_LIBRARY_PATH=/home/mihiraja/.local/lib:$LD_LIBRARY_PATH

# Run the simulation  
exec ./out/gcc-release/IoV-Digital-Twin-TaskOffloading \
    -n .:../inet4.5/src:/opt/omnet/veins/src/veins \
    "$@"
