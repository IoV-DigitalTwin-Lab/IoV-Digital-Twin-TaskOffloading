# IoV Digital Twin Task Offloading - Visual Architecture Guide

## Architecture Overview

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    OMNeT++ SIMULATION ENGINE CORE                           │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  MOBILITY LAYER                    NETWORK LAYER                            │
│  ┌──────────────────────┐          ┌─────────────────────────────────────┐ │
│  │   SUMO Integration   │          │  IEEE 802.11p (DSRC/V2X)           │ │
│  ├──────────────────────┤          ├─────────────────────────────────────┤ │
│  │ ├─ TraCI API         │          │ ├─ 5.9 GHz Frequency              │ │
│  │ ├─ Vehicle Traces    │          │ ├─ OFDM Modulation                │ │
│  │ ├─ Road Network      │          │ ├─ MAC 1609.4 (WAVE)              │ │
│  │ └─ Traffic Flow      │          │ └─ TimeSlot Coordination          │ │
│  └──────────────────────┘          │                                    │ │
│                                     │  PHYSICAL LAYER                   │ │
│                                     │ ├─ Path Loss Model                │ │
│                                     │ ├─ Log-normal Shadowing           │ │
│                                     │ ├─ Obstacle Attenuation           │ │
│                                     │ └─ SNR Calculation                │ │
│                                     └─────────────────────────────────────┘ │
│                                                                              │
│  VEHICLE AGENTS                    RSU EDGE SERVERS                         │
│  ┌──────────────────────┐          ┌─────────────────────────────────────┐ │
│  │ [Vehicle [0..N]]     │          │ [RSU [0..M]] (Edge Compute)         │ │
│  ├──────────────────────┤          ├─────────────────────────────────────┤ │
│  │ ├─ Position tracking │          │ ├─ CPU capacity: 4-8 GHz           │ │
│  │ ├─ Task generation   │          │ ├─ Memory: 8-32 GB                 │ │
│  │ ├─ Heartbeat send    │          │ ├─ Task queue (priority)           │ │
│  │ ├─ Decision receive  │          │ ├─ Admission control               │ │
│  │ └─ Energy model      │          │ ├─ Processing simulation           │ │
│  └──────────────────────┘          │ ├─ Result send back                │ │
│                                     │ └─ Digital Twin updates            │ │
│                                     └─────────────────────────────────────┘ │
│                                                                              │
└────────────────────────────────────────────────────────────────────────────┘
         ↓ (Async Updates)                    ↓ (State Queries)
┌────────────────────────────────────────────────────────────────────────────┐
│                    REDIS DIGITAL TWIN (In-Memory Cache)                    │
├────────────────────────────────────────────────────────────────────────────┤
│ ├─ Vehicle State (pos, speed, cpu%, mem%)                                  │
│ ├─ Task Queue (pending, executing, completed)                              │
│ ├─ RSU Resources (cpu available, queue length)                             │
│ ├─ Service Vehicle Index (sorted by CPU capacity)                          │
│ └─ ML Decision Cache (from PostgreSQL batch jobs)                          │
└────────────────────────────────────────────────────────────────────────────┘
         ↓ (Persistent Writes)                ↓ (Historical Queries)
┌────────────────────────────────────────────────────────────────────────────┐
│              POSTGRESQL DATABASE (Persistence Layer)                        │
├────────────────────────────────────────────────────────────────────────────┤
│ ├─ Task Log (metadata, completion, metrics)                                │
│ ├─ Offloading Decisions (with targets & confidence)                        │
│ ├─ Performance History (for feedback & ML training)                        │
│ └─ ML Model Outputs (DRL decision batches)                                 │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## Task Generation Pipeline

```
TASK TYPE TAXONOMY (6 Core Types)
│
├─ Type 0: LOCAL_OBJECT_DETECTION (Safety-Critical)
│  ├─ Generation: PERIODIC (0.1-0.2s interval)
│  ├─ Size: 512 KB input
│  ├─ Deadline: 0.15-0.25s (tight)
│  ├─ Offloadable: ✗ (must stay local)
│  └─ QoS: 0.90-1.00 (SAFETY_CRITICAL)
│
├─ Type 1: COOPERATIVE_PERCEPTION (V2V Fusion)
│  ├─ Generation: PERIODIC (when vehicles detected)
│  ├─ Size: 256 KB × N vehicles
│  ├─ Deadline: 0.5-0.8s
│  ├─ Offloadable: ✓ (RSU can coordinate)
│  └─ QoS: 0.70-0.89 (HIGH)
│
├─ Type 2: ROUTE_OPTIMIZATION (Path Planning)
│  ├─ Generation: PERIODIC (10-30s per vehicle)
│  ├─ Size: 256 KB (map + traffic)
│  ├─ Deadline: 2.0-5.0s
│  ├─ Offloadable: ✓ (intensive compute)
│  └─ QoS: 0.50-0.69 (MEDIUM)
│
├─ Type 3: FLEET_TRAFFIC_FORECAST (ML Batch)
│  ├─ Generation: BATCH (collect every 60s)
│  ├─ Size: 2-4 MB (trajectory data)
│  ├─ Deadline: 30-50s
│  ├─ Offloadable: ✓ (high parallelization)
│  └─ QoS: 0.20-0.49 (LOW)
│
├─ Type 4: VOICE_COMMAND_PROCESSING (User Input)
│  ├─ Generation: POISSON (λ=0.2 tasks/min)
│  ├─ Size: 64 KB (voice sample)
│  ├─ Deadline: 1.0-2.0s
│  ├─ Offloadable: ✓
│  └─ QoS: 0.70-0.89 (HIGH)
│
└─ Type 5: SENSOR_HEALTH_CHECK (Diagnostics)
   ├─ Generation: PERIODIC (5-10 min)
   ├─ Size: 128 KB (telemetry)
   ├─ Deadline: 10-20s (relaxed)
   ├─ Offloadable: ✓
   └─ QoS: 0.00-0.19 (BACKGROUND)

                     ↓
            
TASK PROFILE DATABASE
│
└─ TaskProfileDatabase[TaskType → Profile]
   ├─ computation { input_min/max, cpu_cycles_min/max, memory }
   ├─ timing { deadline_min/max, qos_value, priority }
   ├─ generation { pattern, period, lambda, jitter }
   └─ offloading { is_offloadable, safety_critical, benefit_ratio }

                     ↓
            
TASK GENERATION (Per Vehicle)
│
├─ VehicleApp::scheduleNextTask()
│  └─ Schedule self-message at t = simTime() + exponential(mean_interval)
│
├─ VehicleApp::handleSelfMsg(taskMsg)
│  ├─ Sample task parameters from distributions
│  ├─ Adjust deadline based on vehicle speed
│  ├─ Create Task object (CREATED state)
│  ├─ Emit signal for metrics
│  └─ Schedule next task arrival
│
└─ Task State: CREATED → ready for transmission

                     ↓

WIRELESS TRANSMISSION
│
├─ DemoBaseApplLayer::sendDown(TaskMetadataMessage)
│
├─ IEEE 802.11p MAC: Queue for transmission
│  ├─ Backoff algorithm
│  ├─ Fragmentation if needed
│  └─ CRC calculation
│
├─ PHY Layer: Transmit over air
│  ├─ Modulation: OFDM (QAM-16, QAM-64)
│  ├─ TX Power: 23 dBm (~200 mW)
│  ├─ TX Time: message_size / bitrate
│  └─ Spectrum: 5.9 GHz, 10 MHz bandwidth
│
└─ Propagation: Path loss, shadowing, fading
```

---

## Offloading Decision Flow

```
NEW TASK METADATA RECEIVED at RSU
│
├─ Extract task characteristics
│  ├─ task_id, cpu_cycles, mem_footprint
│  ├─ deadline_seconds, qos_value
│  ├─ is_safety_critical, is_offloadable
│  └─ priority_level
│
├─ Gather vehicle state (from Redis + recent heartbeat)
│  ├─ local_cpu_available: [0, 4] GHz
│  ├─ local_cpu_utilization: [0.0, 1.0]
│  ├─ local_queue_length: [0, N]
│  └─ local_processing_count: [0, M]
│
├─ Gather RSU state
│  ├─ rsu_cpu_available: [0, 4] GHz
│  ├─ rsu_processing_count: [0, 16]
│  ├─ rsu_max_concurrent: 16 (admission ceiling)
│  └─ rsu_queue_length: [0, K]
│
└─ Gather network state
   ├─ rsu_available: true/false
   ├─ rsu_distance: [0, 500] meters
   ├─ estimated_rssi: [-100, 0] dBm
   └─ estimated_transmission_time: [0.05, 2.0] seconds

            ↓ DECISION ALGORITHM (Heuristic)
            
RULE 1: TRY RSU OFFLOADING
│
├─ is_safety_critical?
│  ├─ YES → PROCEED TO RULE 2 (must execute locally)
│  └─ NO  → Continue
│
├─ Check connectivity
│  ├─ rsu_available && RSSI > -100 dBm?
│  ├─ YES → Continue
│  └─ NO  → PROCEED TO RULE 2
│
├─ Check RSU capacity
│  ├─ rsu_processing_count < rsu_max_concurrent (16)?
│  ├─ YES → OFFLOAD_TO_RSU ✓
│  └─ NO  → PROCEED TO RULE 2
│
└─ Decision Reason: "RSU reachable and has capacity"

            ↓ If Rule 1 fails

RULE 2: TRY LOCAL EXECUTION
│
├─ Check vehicle resources
│  ├─ local_queue_length < 10?
│  ├─ local_cpu_utilization < 0.95?
│  └─ local_mem_available sufficient?
│
├─ ALL TRUE? → EXECUTE_LOCALLY ✓
│
└─ Decision Reason: "RSU unavailable/full, local headroom available"

            ↓ If Rule 2 fails

RULE 3: REJECT TASK
│
├─ REJECT_TASK ✗
│
└─ Decision Reason: "Both local & RSU exhausted, system overloaded"

            ↓

RECORD DECISION
│
├─ decision_maker->decisions_offload++ (or local, reject)
├─ Add to feedback history for ML training
└─ Send OffloadingDecisionMessage back to vehicle

            ↓

IF OFFLOAD_TO_RSU:
│
└─ Task → RSU Priority Queue
   ├─ RSU schedules processing when CPU available
   ├─ Execution time = cpu_cycles / cpu_available_hz
   └─ Send result back to vehicle upon completion

IF EXECUTE_LOCALLY:
│
└─ Task → Vehicle Local Queue
   ├─ Vehicle schedules when local CPU available
   ├─ Execution simulated on vehicle CPU
   └─ No network transmission required
```

---

## Message Flow Diagram (Complete End-to-End)

```
Vehicle [V5]                              RSU [0]                    Node Database
    │                                        │                            │
    │  t=5.23s: Task Generated              │                            │
    │ ─ task_id: V5_T127_5.230000           │                            │
    │ ─ cpu_cycles: 1.8G                    │                            │
    │ ─ deadline: 0.7s                      │                            │
    │ ─ qos_value: 0.75                     │                            │
    │                                        │                            │
    │  t=5.24s: TaskMetadataMessage          │                            │
    │ ─────────────────────────────────────→ │                            │
    │ (IEEE 802.11p broadcast)               │                            │
    │ (1.2 MB @ 6 Mbps = 1.6s transmission)  │                            │
    │                                        │                            │
    │                    t=5.25s: RSU Receives & Processes              │
    │                        ├─ Extract metadata                          │
    │                        ├─ Admission check: ACCEPT ✓               │
    │                        ├─ Add to priority queue                    │
    │                        └─ Update Digital Twin                      │
    │                            │                                        │
    │                            └──────────────────────────────────────→ │
    │                                (Redis: task:V5_T127)                │
    │                                                                      │
    │                    t=5.25-5.65s: RSU Processing                   │
    │                        └─ Queue position: [3] of 4                 │
    │                        t=5.40s: Front task completes               │
    │                        └─ V5_T127 moves to [2]                    │
    │                        t=5.50s: Next task completes               │
    │                        └─ V5_T127 moves to [1]                    │
    │                        t=5.65s: V5_T127 EXECUTION COMPLETES       │
    │                            ├─ Status: COMPLETED_ON_TIME           │
    │                            ├─ Latency: 5.65 - 5.23 = 0.42s        │
    │                            ├─ Energy: 3.5 J                       │
    │                            └─ Free 1.2 MB memory, 0.82 GHz CPU    │
    │                                                                      │
    │                    t=5.65s: Result Available                       │
    │                        ├─ Create TaskResultMessage                 │
    │                        │ ├─ task_id: V5_T127_5.230000            │
    │                        │ ├─ status: COMPLETED_ON_TIME            │
    │                        │ ├─ result_data: [128 KB output]         │
    │                        │ └─ latency: 0.42s                       │
    │                        │                                           │
    │ t=5.85s: TaskResultMessage  │                                      │
    │ ←────────────────────────────                                      │
    │ (IEEE 802.11p unicast)      │                                      │
    │ (128 KB @ 6 Mbps = 170 ms)  │                                      │
    │                                                                      │
    │ TASK COMPLETE!                                                      │
    ├─ Update local metrics                  │                            │
    ├─ Record: completed_on_time++           │                            │
    └─ Emit signal to MetricsManager         │                            │
                                             │                            │
                                        Redis Update                       │
                                             └──────────────────────────→ │
                                        (Redis: task:V5_T127,            │
                                         status = COMPLETED_ON_TIME)      │
```

---

## Task State Machine Diagram

```
                            TASK CREATION
                                  ↓
                            ┌──────────────┐
                            │  CREATED     │
                            └──────┬───────┘
                                   │
                    (Send metadata via 802.11p)
                                   ↓
                            ┌──────────────┐
                            │ METADATA_SENT│
                            └──┬───────┬───┘
                               │       │
                    (RSU decision received)
                               │       │
                ┌──────────────┘       └──────────────┐
                ↓                                      ↓
           ┌────────┐                          ┌──────────┐
           │REJECTED│ (insufficient resources) │ QUEUED   │
           └────────┘                          └────┬─────┘
                                                    │
                                    (Scheduled for processing)
                                                    ↓
                                            ┌──────────────┐
                                            │ PROCESSING   │
                                            └──┬───┬───┬───┘
                                               │   │   │
                    ┌──────────────────────────┘   │   └──────────────────┐
                    │                              │                      │
         (deadline < 0)                   (completes                (completes
                    │                      on time)               after deadline)
                    ↓                              │                     │
            ┌──────────┐                         ↓                      ↓
            │ FAILED   │                    ┌──────────────┐     ┌──────────────┐
            └──────────┘                    │COMPLETED_    │     │COMPLETED_    │
                                            │ON_TIME       │     │LATE          │
                                            └──────────────┘     └──────────────┘
                                                   ↓                     ↓
                                                   └─────────┬──────────┘
                                                            ↓
                                            [METRICS RECORDED] ✓
                                           [RESOURCES RELEASED]
                                          [RSU QUEUE UPDATED]
                                         [DIGITAL TWIN UPDATED]
```

---

## RSU Task Processing Pipeline

```
TASK ARRIVAL
  │
  ├─ Receive TaskMetadataMessage
  ├─ Parse task characteristics
  └─ Extract vehicle state
       │
       ↓
ADMISSION CONTROL
  │
  ├─ Check: rsu_cpu_available > task_cpu_requirement?
  │  └─ NO  → REJECT (return to vehicle)
  │
  ├─ Check: rsu_memory_available > mem_footprint?
  │  └─ NO  → REJECT
  │
  ├─ Check: rsu_processing_count < rsu_max_concurrent (16)?
  │  └─ NO  → REJECT
  │
  └─ YES to all → ACCEPT
       │
       ↓
PRIORITY QUEUE INSERTION
  │
  ├─ Comparator: (QoS DESC, Deadline ASC)
  └─ Insert task in ordered position
       │
       ↓
CPU ALLOCATION CHECK
  │
  ├─ If rsu_cpu_available > 0:
  │  └─ Schedule immediate processing
  │
  └─ Else:
     └─ Wait in queue
          │
          ↓
PROCESSING (when CPU available)
  │
  ├─ Calculate execution time:
  │  └─ exec_time = cpu_cycles / (rsu_cpu_available_hz)
  │
  ├─ Schedule self-message: rsuTaskComplete
  │  └─ After exec_time elapses
  │
  ├─ Update RSU state:
  │  ├─ rsu_processing_count++
  │  ├─ rsu_cpu_available -= allocated_cpu
  │  └─ rsu_memory_available -= mem_footprint
  │
  └─ Monitor while executing...
       │
       ↓
COMPLETION EVENT
  │
  ├─ rsuTaskComplete fires
  ├─ Update metrics
  ├─ Create result message
  └─ Transmit back to vehicle
       │
       ↓
RESOURCE RELEASE
  │
  ├─ rsu_processing_count--
  ├─ rsu_cpu_available += released_cpu
  ├─ rsu_memory_available += mem_footprint
  └─ Update Digital Twin (Redis)
       │
       ↓
CHECK QUEUE
  │
  ├─ Any tasks waiting?
  │  └─ YES → Go back to CPU ALLOCATION CHECK
  │
  └─ NO → Idle (wait for next task)
```

---

## Energy Model Components

```
VEHICLE LOCAL EXECUTION
│
└─ Energy = CPU Energy + Idle Energy
   │
   ├─ CPU Energy
   │  ├─ Cycles: task cpu_cycles
   │  ├─ Frequency: 0.8-2.0 GHz (variable)
   │  ├─ Voltage: 0.6-1.2V (DVFS)
   │  ├─ Current: 100-500 mA
   │  └─ Formula: cycles × (V² × f) × I / f
   │            ≈ 0.5-1.5 J for typical vehicle task
   │
   └─ Idle Energy
      └─ Background draw while waiting in queue

RSU OFFLOAD EXECUTION
│
└─ Energy = Transmission Energy + Processing Energy + RX Energy
   │
   ├─ TX Energy (Vehicle → RSU)
   │  ├─ Packet size: 1.2 MB (task metadata)
   │  ├─ TX Power: 23 dBm (200 mW)
   │  ├─ TX Time: 1.2 MB / 6 Mbps = 1.6 seconds
   │  └─ Energy: 200 mW × 1.6s = 0.32 J
   │
   ├─ RSU Processing Energy
   │  ├─ CPU: 4.0 GHz, 4-5 W typical
   │  ├─ Processing time: 0.4 seconds (1.8G cycles / 4.5 GHz)
   │  ├─ Formula: power × time
   │  └─ Energy: 4W × 0.4s = 1.6 J
   │
   └─ RX Energy (RSU → Vehicle)
      ├─ Packet size: 128 KB (result)
      ├─ RX Power: ~200 mW
      ├─ RX Time: 128 KB / 6 Mbps = 170 ms
      └─ Energy: 200 mW × 0.17s = 0.034 J
   
   TOTAL Offload: 0.32 + 1.6 + 0.034 = 1.95 J ≈ 2.0 J
   (vs. Local: 1.2 J)
   Trade-off: +67% energy for 2.59s vs 3.0+s execution

OVERALL SYSTEM ENERGY
│
└─ Sum across all tasks and vehicles
   Typical distribution:
   ├─ Local execution: ~35% of tasks = 40% of total energy
   ├─ RSU offloading: ~52% of tasks = 58% of total energy
   ├─ Idle/wait energy: ~2% of total
   └─ Total across 500s sim: ~140,000 Joules
```

---

## Latency Components Breakdown

```
LATENCY = Queueing + Transmission + Execution + Propagation

┌─────────────────────────────────────────────────────────────────────┐
│ LOCAL EXECUTION PATH (Vehicle Executes Locally)                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│ Task Generation → Local Queue Wait                                  │
│  ├─ Queueing time: 10-100 ms (depends on load)                     │
│  │                                                                   │
│  └─→ Local Processing                                              │
│      ├─ Execution time: (cpu_cycles / cpu_available_hz)            │
│      ├─ Example: 1.8G / 0.8G = 2.25 seconds                        │
│      └─ Queue wait front loads latency                             │
│                                                                      │
│ TOTAL LOCAL: 100 ms - 3000 ms (depends on queue depth)             │
│  Typical: 0.5 - 2.0 seconds                                         │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│ RSU OFFLOAD PATH (Vehicle Sends to RSU)                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│ 1. Task Metadata TX: Vehicle → RSU                                 │
│    ├─ Size: 1.2 MB                                                 │
│    ├─ Bitrate: 6 Mbps (OFDM modulation)                            │
│    ├─ Time: 1.2 MB / 6 Mbps = 1.6 seconds                          │
│    └─ Propagation delay: ~0.1 ms (negligible)                      │
│                                                                      │
│ 2. RSU Queue Wait                                                   │
│    ├─ Tasks ahead in queue: 0-12                                   │
│    ├─ Each task takes 0.4-0.8 seconds                              │
│    └─ Wait time: 0-10 seconds (worst case overloaded)              │
│                                                                      │
│ 3. RSU Processing                                                   │
│    ├─ Execution time: cpu_cycles / rsu_cpu_available_hz            │
│    ├─ Example: 1.8G / 2.2G = 0.82 seconds                          │
│    └─ CPU shared among concurrent tasks                            │
│                                                                      │
│ 4. Result TX: RSU → Vehicle                                        │
│    ├─ Size: 128 KB (output)                                        │
│    ├─ Bitrate: 6 Mbps                                              │
│    ├─ Time: 128 KB / 6 Mbps = 170 ms                               │
│    └─ Propagation delay: ~0.1 ms (negligible)                      │
│                                                                      │
│ TOTAL OFFLOAD: TX(1.6s) + Queue(0-10s) + Exec(0.8s) + RX(0.17s)   │
│  Best case (no queue): 1.6 + 0.8 + 0.17 = 2.57 seconds             │
│  Average case: 1.6 + 2.0 + 0.8 + 0.17 = 4.57 seconds               │
│  Worst case: 1.6 + 10.0 + 0.8 + 0.17 = 12.57 seconds               │
│                                                                      │
│ Typical range: 2.5 - 5.0 seconds (high variance due to queue)     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

LATENCY COMPARISON
  Task: COOPERATIVE_PERCEPTION
  ├─ Local: 2.3 seconds (vehicle CPU bottleneck)
  └─ Offload: 2.6-5.0 seconds (transmission overhead but faster compute)
              → Trade-off: slightly higher latency but lower energy on vehicle

  Task: ROUTE_OPTIMIZATION (Heavy Compute)
  ├─ Local: 5.0+ seconds (heavy CPU load, queueing)
  └─ Offload: 2.5-3.5 seconds (RSU computes faster despite transmission)
              → Win: offloading beats local by 2-3x

  Task: LOCAL_OBJECT_DETECTION (Safety-Critical)
  ├─ Local: 0.2 seconds (must be local, cannot transmit)
  └─ Offload: Not allowed (forced local execution)
```

---

## Redis Digital Twin State Example

```
Redis Instance @ 127.0.0.1:6379
│
├─ VEHICLE STATES
│  │
│  └─ Hash: vehicle:vehicle_5
│     ├─ pos_x: "1523.4"
│     ├─ pos_y: "4821.2"
│     ├─ speed: "18.5"
│     ├─ heading: "45.0"
│     ├─ cpu_total: "2.0"
│     ├─ cpu_available: "0.6"
│     ├─ cpu_utilization: "0.70"
│     ├─ mem_total: "4096"
│     ├─ mem_available: "2048"
│     ├─ mem_utilization: "0.50"
│     ├─ queue_length: "5"
│     ├─ processing_count: "2"
│     ├─ tasks_generated: "127"
│     ├─ tasks_completed_on_time: "110"
│     ├─ tasks_completed_late: "12"
│     ├─ tasks_failed: "3"
│     ├─ tasks_rejected: "2"
│     ├─ avg_completion_time: "0.420"
│     ├─ deadline_miss_ratio: "0.063"
│     ├─ last_update_time: "5.230"
│     ├─ first_seen_time: "0.050"
│     ├─ ttl: "300" (5 minute expiration)
│     └─ ... (additional fields)
│
├─ TASK STATES
│  │
│  └─ Hash: task:V5_T127_5.230000
│     ├─ vehicle_id: "vehicle_5"
│     ├─ created_time: "5.230"
│     ├─ deadline: "5.930"
│     ├─ state: "COMPLETED_ON_TIME"
│     ├─ decision_type: "OFFLOAD_TO_RSU"
│     ├─ target_id: "RSU0"
│     ├─ completion_time: "5.650"
│     ├─ processing_time: "0.400"
│     ├─ total_latency: "0.420"
│     ├─ ttl: "1800" (30 minute expiration)
│     └─ ... (additional fields)
│
├─ SERVICE VEHICLE INDEX (Sorted Set)
│  │
│  └─ Sorted Set: service_vehicles
│     ├─ Score=2.5 → "vehicle_0" (highest CPU available)
│     ├─ Score=2.1 → "vehicle_3"
│     ├─ Score=2.0 → "vehicle_5"
│     ├─ Score=1.8 → "vehicle_8"
│     └─ ... (sort by CPU score descending)
│
├─ RSU STATES
│  │
│  └─ Hash: rsu:RSU0
│     ├─ pos_x: "1500"
│     ├─ pos_y: "4800"
│     ├─ cpu_total: "4.0"
│     ├─ cpu_available: "2.2"
│     ├─ cpu_utilization: "0.45"
│     ├─ mem_total: "16000"
│     ├─ mem_available: "14000"
│     ├─ memory_utilization: "0.125"
│     ├─ queue_length: "4"
│     ├─ processing_count: "4"
│     ├─ tasks_processed: "287"
│     ├─ avg_latency: "0.380"
│     ├─ last_update_time: "5.650"
│     └─ ttl: "300"
│
├─ ML DECISION CACHE
│  │
│  └─ Hash: decision:V5_T127_5.230000
│     ├─ decision_type: "OFFLOAD_TO_RSU"
│     ├─ target_id: "RSU0"
│     ├─ confidence: "0.95"
│     ├─ decision_time: "5.250"
│     ├─ model_version: "v2.1"
│     └─ ttl: "60" (1 minute cache)
│
└─ OFFLOADING REQUEST QUEUE (List)
   │
   └─ List: offloading:requests
      ├─ [0] {task_id: "V7_T201_5.220", vehicle_id: "vehicle_7",
      │       rsu_id: "RSU0", cpu_cycles: 2100000000,
      │       deadline: 0.85, qos_value: 0.68, ...}
      ├─ [1] {task_id: "V3_T089_5.225", ...}
      └─ ... (batch processed by ML model every 1.0 second)

Key Points:
├─ All data structure use JSON strings or hashes
├─ TTL prevents stale data accumulation
├─ Service vehicle index enables fast V2V offloading lookup
├─ Redis updates asynchronously (non-blocking to simulation)
└─ Query operations O(1) hash lookups, O(log n) sorted set searches
```

---

## Network Topology (Typical Scenario)

```
                              Road Network (7km × 7km)
┌────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  RSU Coverage Area 1          RSU Coverage Area 2                   │
│  ┌──────────────────┐         ┌──────────────────┐                 │
│  │   RSU[0]         │         │   RSU[1]         │                 │
│  │  (1500, 4800)    │         │  (5500, 4800)    │                 │
│  │  ● Coverage: 500m│         │  ● Coverage: 500m│                 │
│  │  ● CPU: 4.0 GHz │         │  ● CPU: 4.0 GHz │                 │
│  │  ● Memory: 16GB  │         │  ● Memory: 16GB  │                 │
│  └───────────┬──────┘         └────────┬─────────┘                 │
│              │                         │                            │
│        ○─────●─────────○              ●─────────────●              │
│      V0  ⟍   V5        V8          V12 ⟍           V18             │
│         ⟍  311m              503m    ⟍                             │
│    ╲    ╲       └─→ OUT OF RANGE     ╲                             │
│ ────╲────●────────────────────────────●───────────────────→         │
│      ╲  V11     Road Main Arterial     V21    Secondary Road      │
│       ● V23                                                         │
│  (2100, 3800)          ┌──────────────────┐                        │
│                        │ Traffic Scenario │                        │
│           ●────●──────●│ ├─ 50 vehicles   │                        │
│          V2  V3  V4   │ ├─ 2 RSUs         │                        │
│ Route to:              │ └─ Dense urban   │                        │
│   (3000,3000)          │   (high handoffs)│                        │
│                        └──────────────────┘                        │
│                                                                      │
│  Network Conditions:                                                │
│  ├─ Frequency: 5.9 GHz (DSRC)                                      │
│  ├─ Bandwidth: 10 MHz per channel                                  │
│  ├─ Path loss model: Friis + shadowing (urban)                     │
│  ├─ Typical RSSI: -70 to -85 dBm (within range)                    │
│  ├─ Out-of-range: < -100 dBm (complete loss)                       │
│  └─ Message success rate: 99.8%                                     │
│                                                                      │
└────────────────────────────────────────────────────────────────────┘

Vehicle Movement Pattern (Example):
├─ V5 Position Over Time
│  ├─ t=0.0s:   (1450, 4750) m, speeding up (0→20 m/s)
│  ├─ t=5.0s:   (1523, 4821) m, 18.5 m/s, approaching RSU
│  ├─ t=10.0s:  (1645, 4892) m, 19.5 m/s, peak speed
│  ├─ t=15.0s:  (1750, 4950) m, 18.0 m/s, moving away
│  ├─ t=20.0s:  (1820, 5005) m, 15.0 m/s, slowing
│  └─ t=30.0s:  (1940, 5100) m, moving out of RSU[0] range → RSU[1]
│
└─ Handoff Scenario
   ├─ V5 loses connection to RSU[0] at t≈22s (distance > 500m)
   ├─ V5 connects to RSU[1] at t≈24s (newly in range)
   └─ Task in-flight during handoff → ERROR handling (retransmit)
```

---

## Metrics Collection & Aggregation

```
SIMULATION RUNTIME ~ 500 seconds

SIGNAL EMISSION (Real-Time)
│
├─ Vehicle Signals
│  ├─ "vehHeartbeat" → emit(1) every 0.5s
│  │  └─ Records: heartbeat count, vehicle ID, position
│  │
│  └─ "taskArrive" → emit(1) per task generated
│     └─ Records: task size, CPU req, deadline, vehicle ID
│
├─ RSU Signals
│  ├─ "rsu_cpu_utilization" → emit(value) every 0.1s
│  │  └─ Records: time-series of CPU %, per RSU
│  │
│  ├─ "rsu_queue_length" → emit(value) every 0.1s
│  │  └─ Records: # tasks in queue over time
│  │
│  └─ "task_completion" → emit(latency) when task completes
│     └─ Records: latency, location (local/RSU), deadline miss
│
└─ System Signals
   ├─ "network_success_rate" → emit(%) on message transmit
   ├─ "total_tasks_completed" → emit(count) at end
   └─ "average_latency" → emit(seconds) at end

            ↓

METRICS MANAGER (Aggregation)
│
└─ Per-Task-Type Aggregation
   │
   ├─ TaskType: LOCAL_OBJECT_DETECTION
   │  ├─ generated: 2,100
   │  ├─ completed_on_time: 2,087
   │  ├─ completed_late: 10
   │  ├─ failed: 3
   │  ├─ rejected: 0
   │  ├─ avg_latency: 0.12 s
   │  ├─ min_latency: 0.08 s
   │  ├─ max_latency: 0.38 s
   │  ├─ total_energy: 18,900 J
   │  ├─ avg_energy: 9.0 J
   │  ├─ on_time_rate: 99.4%
   │  ├─ deadline_miss_rate: 0.6%
   │  └─ local_execution_fraction: 1.0 (100% local due to safety)
   │
   ├─ TaskType: COOPERATIVE_PERCEPTION
   │  ├─ generated: 1,245
   │  ├─ completed_on_time: 1,105
   │  ├─ completed_late: 80
   │  ├─ failed: 45
   │  ├─ rejected: 15
   │  ├─ avg_latency: 0.35 s
   │  ├─ total_energy: 28,600 J
   │  ├─ on_time_rate: 88.8%
   │  ├─ deadline_miss_rate: 10.0%
   │  └─ offload_execution_fraction: 0.97 (mostly offloaded)
   │
   └─ ... (repeat for all 6 task types)

            ↓

SYSTEM-LEVEL AGGREGATES
│
└─ Summary Statistics
   ├─ Total tasks generated: 5,847
   ├─ Completion rate: 89.2%
   ├─ On-time rate: 87.3%
   ├─ Deadline miss rate: 11.0% (late + failed)
   ├─ Average latency (global): 0.38 s
   ├─ Total energy consumed: 142,500 J
   ├─ Offloading benefit: 23% latency reduction vs. all-local
   ├─ Network success: 99.87%
   ├─ RSU utilization: 74.2% average
   └─ Vehicle CPU utilization: 62.1% average

            ↓

RESULT FILES GENERATED
│
├─ results/General-#0.sca (scalar metrics)
│  └─ Contains computed aggregates above
│
├─ results/General-#0.vec (vector time-series)
│  └─ Contains signal emissions over time
│
└─ results/General-#0.vci (index file)
   └─ Fast random access to results
```

---

**This visual guide complements the detailed text presentation and provides quick reference for architecture, flows, and metrics.**

