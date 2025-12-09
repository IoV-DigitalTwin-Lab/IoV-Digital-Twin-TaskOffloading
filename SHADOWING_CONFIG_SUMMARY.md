# Shadowing Configuration Summary

## Your Project Setup Analysis

### ✅ **VEINS Shadowing (Recommended - You're Using This)**

**Location:** Configured via `config.xml` and `obstacles.xml`

**Active Models:**
1. **SimpleObstacleShadowing** - Static building/obstacle attenuation
   - Defined in: `config.xml` lines 12-14
   - Obstacles from: `obstacles.xml` (buildings, vegetation, etc.)
   - Attenuation: Based on material properties (db-per-cut + db-per-meter)
   
**How it works:**
- When signal passes through obstacles in `obstacles.xml`, applies material-based attenuation
- Office buildings: 18 dB entry + 0.9 dB/m penetration
- Apartments: 12 dB entry + 0.6 dB/m penetration
- Single houses: 8 dB entry + 0.4 dB/m penetration
- Clear line-of-sight: No attenuation

**Logging Configuration:**
```ini
# In omnetpp.ini - to see SimpleObstacleShadowing logs:
*.obstacles.cmdenv-log-level = info           # Obstacle module
*.connectionManager.cmdenv-log-level = info   # Connection tracking
*.**.nic.phy80211p.cmdenv-log-level = debug   # PHY layer (for obstacle hits)
```

**Enhanced Logging (just applied):**
- `EV_INFO` logs when obstacles cause attenuation (visible with cmdenv-log-level = info)
- `EV_DEBUG` logs all signal processing including clear line-of-sight
- Shows: positions, distance, power before/after, attenuation in dB

---

### ⚠️ **INET Shadowing (You Have This Configured - May Conflict)**

**Found in your `omnetpp.ini`:**

```ini
# Line 256-259 (General config)
*.inetRadioMedium.pathLoss.shadowingModel.typename = "LogNormalShadowing"
*.inetRadioMedium.pathLoss.shadowingModel.sigma = 6.0dB
*.inetRadioMedium.pathLoss.shadowingModel.correlationDistance = 50m
*.inetRadioMedium.pathLoss.shadowingModel.cmdenv-log-level = debug

# Line 262-266 (Obstacle loss)
*.inetRadioMedium.obstacleLoss.typename = "DielectricObstacleLoss"
*.inetRadioMedium.obstacleLoss.enableReflection = true
*.inetRadioMedium.obstacleLoss.enableRefraction = true
```

**Problem:**
- INET models work with `inetRadioMedium` (separate radio medium from Veins)
- You're using Veins 802.11p radios which use Veins' `ConnectionManager`, NOT `inetRadioMedium`
- These INET shadowing settings likely have **NO EFFECT** on your V2X communication

**Why you have both:**
- Your network extends `Scenario` (Veins) but also includes `inetRadioMedium` submodule
- This is a hybrid setup, but V2X nodes use Veins radio stack

---

## ✅ **Recommendation: Use ONLY Veins Models**

### Current Active Propagation Chain (Veins):
```
config.xml defines the order:
1. SimplePathlossModel (distance-based path loss)
2. SimpleObstacleShadowing (building/obstacle attenuation) ✅
3. NakagamiFading (multipath fading)
```

### What to Disable (INET shadowing):

**Option 1: Comment out INET shadowing (Recommended)**
```ini
# Disable INET shadowing models (not used by Veins V2X radios)
# *.inetRadioMedium.pathLoss.shadowingModel.typename = "LogNormalShadowing"
# *.inetRadioMedium.obstacleLoss.typename = "DielectricObstacleLoss"
```

**Option 2: If you need INET for other nodes (non-V2X):**
Keep INET models but understand they only affect INET radios, not Veins 802.11p radios.

---

## 🔍 **How to Verify Veins Shadowing is Working**

### 1. Enable Enhanced Logging:
```ini
# In omnetpp.ini
*.obstacles.cmdenv-log-level = info
*.connectionManager.cmdenv-log-level = info
*.**.nic.phy80211p.cmdenv-log-level = info
```

### 2. Run simulation and look for:
```
[SimpleObstacleShadowing] OBSTACLE SHADOWING APPLIED
  Original power: -45.2 dBm (0.0000301995 W)
  Attenuation: 22.5 dB (factor: 0.00562341)
  Final power: -67.7 dBm (0.000000169851 W)
  Power loss: 94.38%
```

### 3. Compare scenarios:
- Vehicles behind buildings → High attenuation
- Vehicles in open road → "Clear line-of-sight (no obstacles)"

### 4. Check your obstacles:
```bash
cd /home/mihirajakuruppu/omnetpp-6.1/workspace/complex-network
cat obstacles.xml | grep -A5 "<poly"
```

---

## 📝 **Configuration Files Modified**

### ✅ Enhanced Logging in Veins Source:
- **File:** `/home/mihirajakuruppu/omnetpp-6.1/workspace/veins/src/veins/modules/analogueModel/SimpleObstacleShadowing.cc`
- **Changes:**
  - Added detailed position logging
  - Power levels in both W and dBm
  - Attenuation in dB
  - Clear distinction between obstacle hits and line-of-sight
  - Uses `EV_INFO` for important events (visible with log-level = info)

### 🔧 Need to Recompile Veins:
```bash
cd /home/mihirajakuruppu/omnetpp-6.1/workspace/veins
make clean
make
```

---

## 🎯 **Summary**

| Model | Framework | Status | Used By | Logging |
|-------|-----------|--------|---------|---------|
| **SimpleObstacleShadowing** | Veins | ✅ Active | V2X vehicles | ✅ Enhanced |
| SimplePathlossModel | Veins | ✅ Active | V2X vehicles | Default |
| NakagamiFading | Veins | ✅ Active | V2X vehicles | Default |
| LogNormalShadowing | INET | ⚠️ Configured | (Not used by V2X) | - |
| DielectricObstacleLoss | INET | ⚠️ Configured | (Not used by V2X) | - |

**Your V2X communication uses ONLY Veins models** - the INET shadowing is not affecting your results.
