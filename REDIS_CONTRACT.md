# Redis Data Contract (Step 1)

This document defines the canonical Redis keys/fields currently produced by simulation modules and consumed by the external controller.

## 1) Current Vehicle State

- Key: `vehicle:{vehicle_id}:state`
- Type: Hash
- Producer: `RedisDigitalTwin::updateVehicleState`
- Fields:
  - `pos_x`, `pos_y`, `speed`, `heading`, `acceleration`
  - `cpu_available`, `cpu_utilization`
  - `mem_available`, `mem_utilization`
  - `queue_length`, `processing_count`
  - `source_timestamp`, `last_update`

## 2) Task State

- Key: `task:{task_id}:state`
- Type: Hash
- Producer: `RedisDigitalTwin::createTask`, `updateTaskStatus`, `updateTaskCompletion`
- Typical fields:
  - `vehicle_id`, `status`, `created_time`, `deadline`
  - `task_type`, `is_offloadable`, `is_safety_critical`, `priority_level`
  - completion/update fields as present

## 3) Task Request Payload (controller-facing)

- Key: `task:{task_id}:request`
- Type: Hash
- Producer: `RedisDigitalTwin::pushOffloadingRequest`
- Fields:
  - `task_id`, `vehicle_id`, `rsu_id`
  - `mem_footprint_bytes`, `cpu_cycles`, `deadline_seconds`, `qos_value`, `request_time`
  - `task_type`, `input_size_bytes`, `output_size_bytes`
  - `is_offloadable`, `is_safety_critical`, `priority_level`

## 4) Offloading Queue

- Key: `offloading_requests:queue`
- Type: List
- Producer: `RedisDigitalTwin::pushOffloadingRequest`
- Value: pushed `task_id`

## 5) Secondary Prediction Inputs

- Key: `dt2:pred:{run_id}:latest`
- Type: Hash
- Fields: `cycle_id`, `generated_at`, `horizon_s`, `step_s`, `trajectory_count`

- Key: `dt2:pred:{run_id}:cycle:{cycle_id}:entries`
- Type: Stream
- Fields: `cycle_id`, `vehicle_id`, `step_index`, `predicted_time`, `pos_x`, `pos_y`, `speed`, `heading`, `acceleration`

## 6) Secondary SINR Q Output

- Key: `dt2:q:{run_id}:entries`
- Type: Stream
- Fields: `cycle_index`, `link_type`, `tx_id`, `rx_id`, `step_index`, `predicted_time`, `tx_x`, `tx_y`, `rx_x`, `rx_y`, `distance_m`, `sinr_db`
- `link_type` values: `V2V` (vehicle→vehicle UL), `V2RSU` (vehicle→RSU UL), `RSU2V` (RSU→vehicle DL)
- Uplink key format: `{link_type}:{tx_id}:{rx_id}`
- Reverse/downlink lookup:
  - V2V UL `V2V:SRC:DST`  → DL `V2V:DST:SRC`
  - V2RSU UL `V2RSU:SRC:RSU` → DL `RSU2V:RSU:SRC`

- Key: `dt2:q:{run_id}:latest`
- Type: Hash
- Fields: `cycle_index`, `sim_time`, `horizon_s`, `step_s`, `sinr_threshold_db`, `trajectory_count`, `entry_count`, `generated_at`

## 7) Decision Output (for RSU polling)

- Key: `task:{task_id}:decision`
- Type: Hash
- Consumer: `MyRSUApp` decision poller (`RedisDigitalTwin::getDecision`)
- Minimal fields expected:
  - `decision_type`
  - `target_id` (for service-vehicle path)

## Units

- `input_size_bytes`, `output_size_bytes`, `mem_footprint_bytes`: bytes
- `cpu_cycles`: cycles
- `cpu_available`: expected Hz in current simulation messages
- `sinr_db`: dB
- `predicted_time` and all time fields: seconds
