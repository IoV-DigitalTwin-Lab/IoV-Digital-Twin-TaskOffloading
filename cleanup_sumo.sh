#!/bin/bash
# Cleanup script for zombie SUMO/TraCI processes
# Run this if you get "Connection to TraCI server lost" errors

echo "Cleaning up SUMO and TraCI processes..."

# Kill any running sumo processes
pkill -f "sumo" 2>/dev/null
pkill -f "sumo-gui" 2>/dev/null
pkill -f "sumo-launchd" 2>/dev/null

# Wait a moment for processes to terminate
sleep 1

# Check if port 9999 is still in use
if lsof -i :9999 >/dev/null 2>&1; then
    echo "WARNING: Port 9999 is still in use. Trying to force kill..."
    # Get PID using port 9999 and kill it
    PID=$(lsof -t -i :9999)
    if [ -n "$PID" ]; then
        kill -9 $PID 2>/dev/null
        sleep 1
    fi
fi

# Verify cleanup
if lsof -i :9999 >/dev/null 2>&1; then
    echo "ERROR: Unable to free port 9999. Manual intervention required."
    lsof -i :9999
    exit 1
else
    echo "✓ Port 9999 is free"
fi

# Check for any remaining SUMO processes
if pgrep -f "sumo" >/dev/null 2>&1; then
    echo "WARNING: Some SUMO processes are still running:"
    ps aux | grep -E "sumo" | grep -v grep
else
    echo "✓ No SUMO processes running"
fi

echo ""
echo "Cleanup complete. You can now run your simulation."
