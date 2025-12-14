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

# For GUI mode (default):
echo "Starting simulation in GUI mode..."
./complex-network "$@"

# For command-line mode, uncomment this instead:
# echo "Starting simulation in command-line mode..."
# ./complex-network -u Cmdenv "$@"
