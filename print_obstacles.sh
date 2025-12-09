#!/bin/bash
# Obstacle Location Printer for Complex Network Simulation
# This script extracts and displays obstacle locations from obstacles.xml

echo "===== COMPLEX NETWORK OBSTACLE LOCATIONS ====="
echo "Extracting obstacle information from obstacles.xml..."
echo ""

if [ -f "obstacles.xml" ]; then
    echo "📍 OBSTACLE SUMMARY:"
    echo "===================="
    
    # Count total obstacles
    obstacle_count=$(grep -c "<poly id=" obstacles.xml)
    echo "Total Obstacles: $obstacle_count"
    echo ""
    
    echo "🏢 COMMERCIAL DISTRICT (Blue tones):"
    echo "------------------------------------"
    grep -A1 "office_tower_1\|office_complex\|shopping_mall_main" obstacles.xml | grep "shape=" | while read line; do
        id=$(echo "$line" | grep -o 'id="[^"]*"' | cut -d'"' -f2)
        shape=$(echo "$line" | grep -o 'shape="[^"]*"' | cut -d'"' -f2)
        echo "  - $id: $shape"
    done
    echo ""
    
    echo "🏠 RESIDENTIAL AREAS (Brown tones):"
    echo "-----------------------------------"
    grep -A1 "house_cluster\|apartment_" obstacles.xml | grep "shape=" | while read line; do
        id=$(echo "$line" | grep -o 'id="[^"]*"' | cut -d'"' -f2)
        shape=$(echo "$line" | grep -o 'shape="[^"]*"' | cut -d'"' -f2)
        echo "  - $id: $shape"
    done
    echo ""
    
    echo "🏭 INDUSTRIAL ZONE (Gray tones):"
    echo "--------------------------------"
    grep -A1 "warehouse_complex" obstacles.xml | grep "shape=" | while read line; do
        id=$(echo "$line" | grep -o 'id="[^"]*"' | cut -d'"' -f2)
        shape=$(echo "$line" | grep -o 'shape="[^"]*"' | cut -d'"' -f2)
        echo "  - $id: $shape"
    done
    echo ""
    
    echo "🌳 GREEN SPACES (Green tones):"
    echo "------------------------------"
    grep -A1 "central_park\|street_trees" obstacles.xml | grep "shape=" | while read line; do
        id=$(echo "$line" | grep -o 'id="[^"]*"' | cut -d'"' -f2)
        shape=$(echo "$line" | grep -o 'shape="[^"]*"' | cut -d'"' -f2)
        echo "  - $id: $shape"
    done
    echo ""
    
    echo "🅿️ INFRASTRUCTURE (Dark tones):"
    echo "--------------------------------"
    grep -A1 "parking\|overpass\|gas_station\|strip_mall" obstacles.xml | grep "shape=" | while read line; do
        id=$(echo "$line" | grep -o 'id="[^"]*"' | cut -d'"' -f2)
        shape=$(echo "$line" | grep -o 'shape="[^"]*"' | cut -d'"' -f2)
        echo "  - $id: $shape"
    done
    echo ""
    
    echo "📊 COORDINATE RANGES:"
    echo "--------------------"
    echo "Network Boundary: X(-100 to 200), Y(-200 to 100)"
    echo "Playground Size: 400m x 400m"
    echo ""
    
    echo "🎯 RSU LOCATIONS:"
    echo "-----------------"
    echo "RSU[0]: X=-50, Y=10, Z=3 (Near junction J11)"
    echo "RSU[1]: X=50, Y=-50, Z=3 (Near junction J14)"
    echo ""
    
else
    echo "❌ ERROR: obstacles.xml file not found!"
    echo "Please ensure you're in the correct directory."
fi

echo "============================================="
echo "Run simulation with: ./complex-network -c Phase4-UrbanEnvironment"
echo "Enhanced logging will show obstacle loading details."
echo "============================================="