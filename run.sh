#!/bin/bash
# ============================================================================
# Run OMNeT++ Simulation with Environment Variables
# ============================================================================
# This script loads environment variables from .env and runs the simulation
# ============================================================================

# Set library paths for locally installed dependencies
export LD_LIBRARY_PATH=/home/mihiraja/.local/lib:$LD_LIBRARY_PATH

# Check if .env file exists and load it
if [ -f .env ]; then
    echo "Loading environment variables from .env..."
    export $(grep -v '^#' .env | grep -v '^$' | xargs)
else
    echo "Warning: .env file not found. Using system environment variables."
    echo "To create .env, copy the template below:"
    echo ""
    echo "DATABASE_URL=postgresql://postgres:password@localhost:5432/iov_digital_twin"
    echo ""
fi

# Check if DATABASE_URL is set
if [ -z "$DATABASE_URL" ]; then
    echo "ERROR: DATABASE_URL not found"
    echo "Please set DATABASE_URL environment variable or create a .env file"
    echo ""
    echo "Example .env file:"
    echo "DATABASE_URL=postgresql://postgres:password@localhost:5432/iov_digital_twin"
    exit 1
fi

echo "DATABASE_URL is configured"
echo ""

# Cleanup any zombie SUMO processes that might block TraCI connection
echo "Checking for zombie SUMO processes..."
pkill -f "sumo-launchd" 2>/dev/null
if lsof -i :9999 >/dev/null 2>&1; then
    echo "Port 9999 is in use. Cleaning up..."
    PID=$(lsof -t -i :9999 2>/dev/null)
    if [ -n "$PID" ]; then
        kill $PID 2>/dev/null
        sleep 1
    fi
fi
echo "✓ TraCI port (9999) is ready"
echo ""

# Set library paths for locally installed dependencies
export LD_LIBRARY_PATH=/home/mihiraja/.local/lib:$LD_LIBRARY_PATH

# Set SUMO environment variable for TraCI connection
export SUMO_HOME=/usr/share/sumo

# Determine the executable path
EXECUTABLE="./out/gcc-release/IoV-Digital-Twin-TaskOffloading"
if [ ! -f "$EXECUTABLE" ]; then
    echo "ERROR: Executable not found at $EXECUTABLE"
    echo "Please build the project first: make MODE=release"
    exit 1
fi

# Run the simulation
# Use one of these depending on your preference:

# For GUI mode (default):
echo "Starting simulation in GUI mode..."
$EXECUTABLE "$@"

# For command-line mode, uncomment this instead:
# echo "Starting simulation in command-line mode..."
# $EXECUTABLE -u Cmdenv "$@"
