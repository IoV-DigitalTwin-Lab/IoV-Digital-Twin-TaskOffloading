#!/bin/bash
# ============================================================================
# Run OMNeT++ Simulation with Environment Variables
# ============================================================================
# This script loads environment variables from .env and runs the simulation
# ============================================================================

# Check if .env file exists
if [ ! -f .env ]; then
    echo "ERROR: .env file not found!"
    echo "Please copy .env.example to .env and configure your credentials"
    exit 1
fi

# Load environment variables from .env file
echo "Loading environment variables from .env..."
export $(grep -v '^#' .env | xargs)

# Check if DATABASE_URL is set
if [ -z "$DATABASE_URL" ]; then
    echo "ERROR: DATABASE_URL not found in .env file"
    exit 1
fi

echo "DATABASE_URL is configured"
echo ""

# Run the simulation
# Use one of these depending on your preference:

# Set framework paths (relative defaults for clone-and-run)
INET_PATH="${INET_PATH:-${INET_PROJ:-../../inet4.5}}"
VEINS_PATH="${VEINS_PATH:-${VEINS_PROJ:-../../veins-5.3.1}}"

# For GUI mode (default):
echo "Starting simulation in GUI mode..."
./IoV-Digital-Twin-TaskOffloading -n .:$INET_PATH/src:$VEINS_PATH/src/veins -l $INET_PATH/src/INET -l $VEINS_PATH/src/veins "$@"

# For command-line mode, uncomment this instead:
# echo "Starting simulation in command-line mode..."
# ./IoV-Digital-Twin-TaskOffloading -u Cmdenv -n .:$INET_PATH/src:$VEINS_PATH/src/veins -l $INET_PATH/src/INET -l $VEINS_PATH/src/veins "$@"
