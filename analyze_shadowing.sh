#!/bin/bash

# === SHADOWING EFFECT ANALYSIS SCRIPT ===
# This script analyzes the simulation logs to show how obstacles affect shadowing

echo "=== Obstacle Shadowing Effect Analysis ==="
echo "Analyzing simulation logs for shadowing effects..."
echo

# Check if log files exist
if [ ! -f "results/Phase4-UrbanEnvironment-#0.vec" ] && [ ! -f "results/General-#0.vec" ]; then
    echo "No simulation results found. Please run the simulation first:"
    echo "cd /home/mihirajakuruppu/omnetpp-6.1/workspace/complex-network"
    echo "./ComplexNetwork -c Phase4-UrbanEnvironment"
    exit 1
fi

echo "1. OBSTACLE LOCATIONS AND PROPERTIES:"
echo "======================================"
if [ -f "obstacles.xml" ]; then
    grep -A 2 'poly id=' obstacles.xml | grep -E 'shape=|type=' | while read line; do
        echo "   $line"
    done
else
    echo "   obstacles.xml not found"
fi
echo

echo "2. SIGNAL PATH LOSS ANALYSIS:"
echo "============================="
if ls results/*.vec >/dev/null 2>&1; then
    echo "   Analyzing path loss variations..."
    grep -h "pathLoss\|obstacle\|shadowing" results/*.vec 2>/dev/null | head -20
    if [ $? -ne 0 ]; then
        echo "   No path loss data found in vector files"
    fi
else
    echo "   No vector files found"
fi
echo

echo "3. OBSTACLE INTERSECTION ANALYSIS:"
echo "=================================="
if ls results/*.sca >/dev/null 2>&1; then
    echo "   Checking for obstacle interactions..."
    grep -h "obstacle\|loss\|attenuation" results/*.sca 2>/dev/null | head -15
    if [ $? -ne 0 ]; then
        echo "   No obstacle interaction data found in scalar files"
    fi
else
    echo "   No scalar files found"
fi
echo

echo "4. SHADOWING EFFECT SUMMARY:"
echo "==========================="
echo "   Signal propagation is affected by:"
echo "   • LogNormalShadowing: σ = 6.0-8.0 dB (depending on environment)"
echo "   • TwoRayGroundReflection: Path loss exponent α = 4.0"
echo "   • DielectricObstacleLoss: Material-based attenuation"
echo "   • Office buildings: 18 dB/cut + 0.9 dB/meter attenuation"
echo

echo "5. REAL-TIME MONITORING COMMANDS:"
echo "================================="
echo "   To monitor live shadowing effects during simulation:"
echo "   tail -f results/General-#0.elog | grep -E 'obstacle|shadowing|pathLoss'"
echo
echo "   To run simulation with detailed logging:"
echo "   ./ComplexNetwork -c Phase4-UrbanEnvironment -u Qtenv"
echo

echo "Analysis complete. Run the simulation to generate detailed shadowing logs."