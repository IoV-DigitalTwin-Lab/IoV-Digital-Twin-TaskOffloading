#!/bin/bash
# Run the IoV Digital Twin Task Offloading simulation
# This script sources the OMNeT++ environment and runs the simulator with correct NED paths

set -e

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source OMNeT++ environment
OMNETPP_HOME="${OMNETPP_HOME:-/home/mjavin/omnetpp-6.1}"
if [ -f "$OMNETPP_HOME/setenv" ]; then
    source "$OMNETPP_HOME/setenv"
else
    echo "Error: Cannot find OMNeT++ at $OMNETPP_HOME"
    exit 1
fi

# Navigate to project directory
cd "$SCRIPT_DIR"

# Framework paths (relative to project)
INET_PATH="../../inet4.5"
VEINS_PATH="../../veins-5.3.1"

# Run the simulation
echo "Starting IoV Digital Twin Task Offloading simulation..."
echo "NED path: .:$INET_PATH/src:$VEINS_PATH/src/veins"
echo ""

./IoV-Digital-Twin-TaskOffloading \
  -n ".:$INET_PATH/src:$VEINS_PATH/src/veins" \
  -l "$INET_PATH/src/INET" \
  -l "$VEINS_PATH/src/veins" \
  omnetpp.ini "$@"
