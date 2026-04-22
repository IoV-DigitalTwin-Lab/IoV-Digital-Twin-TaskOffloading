# Vehicle Shadowing Experiment Plan (Secondary Simulation Only)

## 1. Goal

This document describes how to run a separate experiment in the secondary simulation path to show why SINR should be re-evaluated over time instead of using a static lookup table.

Main claim to demonstrate:
- For one fixed Tx/Rx location pair, large-scale path loss can be deterministic.
- In realistic traffic, effective SINR is still time-varying because dynamic vehicle blockers change over time.
- Therefore, transmission time estimates for offloading must use time-varying SINR (per step / per cycle), not a single precomputed value.

Scope lock for this experiment:
- Use the secondary simulation only.
- Disable interference contribution (interference model OFF).
- Focus on shadowing-only impact.

This README now maps to implemented files and runnable commands.

## 2. Current Environment Facts (Verified)

- OMNeT++ uses `erlangen.sumo.cfg` from `omnetpp.ini`.
- SUMO scenario uses:
  - `erlangen.net.xml`
  - `erlangen.rou.xml`
  - `erlangen.poly.xml`
- Secondary SINR path in `MyRSUApp.cc` already models:
  - Distance/path-loss term
  - Static obstacle loss from polygons
  - Dynamic vehicle blocking loss
  - Lognormal shadowing

For this experiment, interference will be disabled so observed differences come from shadowing terms only.

Implication: your argument is technically valid. SINR is not fully deterministic in this setup because blockers and interference are dynamic.

## 3. Experiment Design

### 3.1 Two Runs (same seed and same links)

Run A (With vehicle shadowing):
- Use mixed traffic with extra background vehicles (about 200).
- Enable dynamic vehicle-blocking loss in SINR computation.
- Keep interference disabled.

Run B (Without vehicle shadowing):
- Keep exactly the same traffic, trajectories, seed, and communicating pair.
- Disable only vehicle-blocking attenuation term in SINR computation.
- Keep path loss and static obstacle settings unchanged.
- Keep interference disabled here as well.

This isolates the effect of vehicle shadowing.

### 3.2 Communicating Entities (Both Scenarios Required)

- Keep existing communication nodes unchanged.
- Select links for both communication modes:
  - One stable V2V pair (Vehicle A -> Vehicle B)
  - One stable V2RSU pair (Vehicle C -> RSU k)
- Use the exact same V2V and V2RSU links in both Run A and Run B.

### 3.3 Additional Background Vehicles

- Add about 200 extra vehicles as mobility-only traffic objects.
- They must follow valid roads/edges in `erlangen.net.xml`.
- They should include mixed classes:
  - passenger car
  - bus
  - truck
  - van
- They do not need application-layer task offloading logic.

Important:
- If your vehicle-shadowing model depends on vehicle geometry from TraCI (length/width/height), these background vehicles must still be visible to TraCI at runtime.

## 4. Implemented Assets

To avoid breaking current experiments, keep this as a parallel scenario.

### 4.1 SUMO Route File (Implemented)

Implemented file:
- `erlangen.shadowing_bg.rou.xml`

Contains:
- existing baseline traffic retained
- +200 additional background vehicles or flows
- mixed vType definitions (car/bus/truck/van)

Design used:
- 20 background flows x 10 vehicles each = 200 additional moving blockers.
- Routes are defined from existing Erlangen edges to stay on valid roads.

### 4.2 SUMO Config (Implemented)

Implemented file:
- `erlangen.shadowing_exp.sumo.cfg`

It points to:
- `erlangen.net.xml`
- `erlangen.rou.xml,erlangen.shadowing_bg.rou.xml`
- `erlangen.poly.xml`

Validation done:
- Short SUMO dry run with this config exits successfully (exit code 0).

### 4.3 OMNeT Configs (Implemented)

Implemented in `omnetpp.ini`:

- `VehicleShadowingExp_With`
  - uses `erlangen.shadowing_exp.sumo.cfg` via `*.manager.configFile`
  - vehicle-blocking loss enabled
  - interference model disabled
  - run id: `DT-Secondary-Shadowing-With`

- `VehicleShadowingExp_Without`
  - same as above, but vehicle-blocking loss disabled
  - interference model disabled
  - run id: `DT-Secondary-Shadowing-Without`

Both configs keep identical:
- RNG seed
- simulation duration
- V2V and V2RSU link selection
- PHY/MAC settings (except the single shadowing switch)

Additional fixed controls in both configs:
- `secondaryInterferenceMw = 0`
- `secondaryNakagamiEnabled = false`
- `secondarySinrThresholdDb = -30` (retain low-SINR samples for plotting)

### 4.4 Vehicle Shadowing Toggle (Implemented)

Implemented in code and NED:
- `MyRSUApp.ned`: `secondaryVehicleShadowingEnabled` parameter added.
- `MyRSUApp.h` / `MyRSUApp.cc`: toggle wired into secondary SINR pipeline.

Behavior:
- `true`: dynamic vehicle-blocking attenuation is applied.
- `false`: vehicle-blocking attenuation is skipped.

### 4.5 Interference Model OFF (Applied)

Apply this rule in both A/B configs:
- Disable secondary interference contribution in SINR calculation.
- Keep this setting identical in A and B to isolate shadowing.

Practical note:
- If your implementation uses an interference activity factor/parameter (for example an interference ratio), set it to zero in both configs.

### 4.6 Route/Road Alignment Requirement

To ensure all new vehicles are valid on existing roads:
- generate trips/routes from `erlangen.net.xml` (not hand-placed random coordinates)
- validate with SUMO tools (`duarouter` or `randomTrips.py` + route build)
- run `sumo --check-routes` on the experiment config

Acceptance condition:
- zero invalid-edge or disconnected-route warnings.

## 5. What to Plot (Core Figures)

You asked what exactly to plot to make the argument clear. Use these core plots.

### Plot 1 (Most important): Path Loss vs Time (A/B overlay)

For each selected link type (V2V and V2RSU):
- x-axis: simulation time
- y-axis: effective path loss (dB)
- lines:
  - With vehicle shadowing
  - Without vehicle shadowing

Expected result:
- Without vehicle shadowing: smoother curve.
- With vehicle shadowing: additional dips/spikes due to moving blockers.

### Plot 2 (Most important): SINR vs Time (A/B overlay)

For each selected link type (V2V and V2RSU):
- x-axis: simulation time
- y-axis: SINR (dB)
- lines:
  - With vehicle shadowing
  - Without vehicle shadowing

Expected result:
- Mean may be close, but instantaneous variance and short fades increase in the shadowing case.

### Plot 3: Data Rate vs Time (A/B overlay)

For each selected link type (V2V and V2RSU):
- x-axis: simulation time
- y-axis: effective data rate (Mbps)
- lines:
  - With vehicle shadowing
  - Without vehicle shadowing

Rate conversion:
- Use the same SINR-to-rate model already used in your pipeline.
- Keep bandwidth/efficiency/cap identical for A and B.

Expected result:
- Shadowing case shows deeper short-term rate drops and higher variance.

### Plot 4: SINR CDF (distribution comparison)

- x-axis: SINR (dB)
- y-axis: CDF
- curves:
  - With vehicle shadowing
  - Without vehicle shadowing

Expected result:
- Shadowing curve should show heavier low-SINR tail.

### Plot 5: Transmission Time per Task (A/B boxplot or CDF)

For offloaded tasks mapped to the same pair/time windows:
- transmission time computed from link rate derived from SINR

Expected result:
- With shadowing: more spread, higher tail latency.

This directly links radio effect to offloading QoS.

## 6. Recommended Additional Metrics

- Number of blocking vehicles intersecting LoS per sample
- Fraction of time SINR < threshold (outage ratio)
- Fraction of time data rate < required threshold
- 95th percentile transmission time
- Mean and standard deviation of SINR for A/B runs
- Mean and standard deviation of data rate for A/B runs

These make the “subtle difference” measurable, not only visual.

## 7. Data Logging Plan

For each sampled point, log at least:
- run_mode (`with_shadowing` / `without_shadowing`)
- link_mode (`V2V` / `V2RSU`)
- sim_time
- tx_id, rx_id
- tx_x, tx_y, rx_x, rx_y
- distance_m
- path_loss_db
- vehicle_shadow_loss_db
- obstacle_loss_db
- interference_mw (expected to be 0 for this study)
- sinr_db
- estimated_rate_bps
- estimated_tx_time_s (for representative payload sizes)

Use the same sampling interval in both runs.

## 8. Fairness Controls (Very Important)

For strict A/B validity, keep fixed:
- random seed
- traffic routes and departure schedules
- simulation runtime window
- selected V2V link and selected V2RSU link
- all radio parameters except vehicle-shadowing toggle
- interference setting fixed to OFF in both runs

Only one factor should change between runs.

## 9. Interpretation Guide for Supervisor Discussion

Short answer you can use:

- If geometry and environment were fully static, a deterministic precomputed SINR table could be acceptable.
- In our case, moving vehicles create time-varying blockage, so instantaneous SINR changes even for similar distances.
- Because offloading delay depends on link rate from SINR, dynamic SINR evaluation is required to avoid optimistic or biased transmission-time estimates.

## 10. Step-by-Step Execution Checklist (Implementation Phase)

- [x] Create experiment route file with +200 mixed background vehicles.
- [x] Create experiment SUMO config using that route file.
- [x] Add two OMNeT configs (`with` and `without` vehicle shadowing).
- [x] Disable interference model in both configs.
- [x] Add plotting script for V2V and V2RSU (including data rate vs time).
- [x] Add one-command run helper for A/B + plotting.
- [ ] Run A and B with same seed on your machine.
- [ ] Review generated plots and report mean/std/p95 deltas.

## 11. Plot Generation Workflow (Implemented)

Implemented files:
- `plot_shadowing_experiment.py`
- `run_shadowing_experiment.sh`

Recommended outputs:
- `plots/v2v_pathloss_vs_time.png`
- `plots/v2v_sinr_vs_time.png`
- `plots/v2v_rate_vs_time.png`
- `plots/v2r_pathloss_vs_time.png`
- `plots/v2r_sinr_vs_time.png`
- `plots/v2r_rate_vs_time.png`
- `plots/sinr_cdf_compare.png`
- `plots/tx_time_compare.png`

Recommended script behavior:
- Input: Redis streams `dt2:q:<run_id>:entries` from Run A and Run B.
- Filter: selected V2V pair and selected V2RSU pair (manual args or auto-select by sample count).
- Compute:
  - `rate_mbps = estimated_rate_bps / 1e6`
  - tx-time distribution from SINR-derived rates
- Save figures with consistent axes, legends, and units.

Plotting standards:
- Always include axis labels and units.
- Use same y-axis range for A/B of the same metric.
- Keep V2V and V2RSU in separate figures for clarity.
- Export at high resolution (for reports).

Run commands:

1) Run A/B and generate plots automatically:
```bash
./run_shadowing_experiment.sh
```

2) Generate only plots from existing Redis data:
```bash
python3 ./plot_shadowing_experiment.py \
  --with-run-id DT-Secondary-Shadowing-With \
  --without-run-id DT-Secondary-Shadowing-Without
```

3) Pin exact links for comparison (recommended for report reproducibility):
```bash
python3 ./plot_shadowing_experiment.py \
  --v2v-tx v10 --v2v-rx v17 \
  --v2r-tx v10 --v2r-rx RSU_0
```

## 12. Expected Outcome

You should be able to show:
- similar large-scale trend from distance/path loss,
- but non-negligible short-term fluctuations and tail degradation when dynamic vehicle shadowing is enabled,
- and corresponding impact on transmission time used in offloading decisions.

That is the key evidence for why repeated SINR calculation is justified in this digital-twin offloading pipeline.