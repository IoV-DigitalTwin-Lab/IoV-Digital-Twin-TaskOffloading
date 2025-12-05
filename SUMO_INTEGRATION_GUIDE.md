# Phase 4: SUMO-VEINS Environmental Integration Guide

## Overview
This file provides guidance for integrating building data from SUMO network files into the Veins/INET environmental modeling system for Phase 4 implementation.

## SUMO Building Data Integration

### 1. SUMO Additional File Format
Add buildings to your SUMO scenario using additional files:

```xml
<!-- sumo_buildings.add.xml -->
<additionalFile>
    <polygons>
        <!-- Commercial Buildings -->
        <polygon id="bld_commercial_1" type="building.commercial" color="gray" fill="1" layer="0">
            400,900 600,900 600,1100 400,1100
        </polygon>
        <polygon id="bld_commercial_2" type="building.commercial" color="gray" fill="1" layer="0">
            1400,1100 1600,1100 1600,1300 1400,1300
        </polygon>
        
        <!-- Residential Buildings -->
        <polygon id="bld_residential_1" type="building.residential" color="brown" fill="1" layer="0">
            1900,400 2050,400 2050,550 1900,550
        </polygon>
        
        <!-- Parks and Vegetation -->
        <polygon id="park_1" type="greenarea" color="green" fill="1" layer="0">
            1150,750 1250,750 1250,850 1150,850
        </polygon>
        
        <!-- Parking Areas -->
        <polygon id="parking_1" type="parking" color="orange" fill="1" layer="0">
            435,1135 465,1135 465,1165 435,1165
        </polygon>
    </polygons>
</additionalFile>
```

### 2. SUMO Configuration Integration
Update your .sumocfg file to include building data:

```xml
<configuration>
    <input>
        <net-file value="ComplexNetwork.net.xml"/>
        <route-files value="ComplexNetwork.rou.xml"/>
        <additional-files value="sumo_buildings.add.xml"/>
    </input>
    <!-- ... other configuration ... -->
</configuration>
```

### 3. Coordinate System Alignment
Ensure SUMO and OMNeT++ coordinate systems are aligned:

**In omnetpp.ini:**
```ini
# Coordinate system alignment
*.manager.margin = 25
*.manager.roi = "-1, -1, 2501, 2501"  # Match SUMO network bounds
*.physicalEnvironment.coordinateSystem.typename = "SimpleCoordinateSystem"

# Playground size matching SUMO network
*.playgroundSizeX = 2500m
*.playgroundSizeY = 2500m
*.playgroundSizeZ = 50m
```

### 4. Dynamic Vehicle Obstacle Integration
For dynamic vehicle obstacles (traffic jams, parked cars):

**SUMO Vehicle Data:**
```xml
<!-- Dynamic vehicle positions as temporary obstacles -->
<vehicleType id="passenger" accel="2.6" decel="4.5" sigma="0.5" length="4.5" minGap="2.5" maxSpeed="55.56"/>
<vehicleType id="truck" accel="1.3" decel="4.0" sigma="0.5" length="12.0" minGap="3.0" maxSpeed="39.44"/>
```

**Veins Integration:**
```cpp
// In vehicle application code - report position for obstacle modeling
if (isParked() || isInTrafficJam()) {
    reportVehicleAsObstacle(mobility->getPositionAt(simTime()));
}
```

## Building Type Mapping

### SUMO to Veins/INET Mapping:
```
SUMO Type              -> Veins Obstacle Type     -> INET Material
building.commercial    -> commercial_building     -> reinforced_concrete
building.residential   -> residential_building    -> brick  
building.industrial    -> industrial_building     -> metal_sheet
greenarea             -> dense_vegetation        -> dense_foliage
parking               -> vehicle_cluster         -> metal
```

## Advanced Features

### 1. Height Information Integration
Extract building heights from SUMO or OSM data:

```xml
<!-- Enhanced SUMO building with height -->
<polygon id="bld_1" type="building.commercial" color="gray" fill="1" layer="0" height="25.0">
    400,900 600,900 600,1100 400,1100
</polygon>
```

### 2. Material Property Assignment
Automatically assign material properties based on building type:

```cpp
// Building type to material mapping
std::map<std::string, std::string> materialMap = {
    {"building.commercial", "reinforced_concrete"},
    {"building.residential", "brick"},
    {"building.industrial", "metal_sheet"},
    {"greenarea", "dense_foliage"}
};
```

### 3. Real-time Obstacle Updates
For dynamic environments:

```ini
# Enable dynamic obstacle updates
*.obstacles.enableDynamicObstacles = true
*.obstacles.updateInterval = 1s
*.manager.updateInterval = 0.5s  # Faster SUMO updates
```

## Validation and Testing

### 1. Visual Verification
- Use SUMO-GUI to verify building placement
- Check OMNeT++ Tkenv for obstacle visualization
- Verify coordinate alignment between systems

### 2. Signal Propagation Testing
- Test signal strength around buildings
- Verify shadowing effects match building geometry
- Check ground reflection calculations

### 3. Performance Monitoring
- Monitor simulation speed with dense obstacles
- Optimize obstacle update frequency
- Use spatial indexing for large environments

## Best Practices

### 1. Coordinate System
- Always verify SUMO network bounds match OMNeT++ playground
- Use consistent coordinate origins
- Test with known reference points

### 2. Building Density
- Start with sparse building placement
- Gradually increase density for performance testing
- Use building type hierarchy (commercial > residential > vegetation)

### 3. Dynamic Integration
- Update obstacles only when vehicles stop/park
- Use spatial clustering for multiple parked vehicles
- Implement obstacle lifetime management

## Troubleshooting

### Common Issues:
1. **Coordinate Misalignment**: Check playground size and SUMO bounds
2. **Performance Issues**: Reduce obstacle update frequency
3. **Missing Buildings**: Verify additional file loading in SUMO
4. **Signal Issues**: Check material property assignments

### Debug Commands:
```bash
# Test SUMO building loading
sumo-gui -c ComplexNetwork.sumocfg --additional-files sumo_buildings.add.xml

# Verify OMNeT++ obstacle loading
./complex-network -u Cmdenv -c Phase4-UrbanEnvironment --debug-on-errors=true
```