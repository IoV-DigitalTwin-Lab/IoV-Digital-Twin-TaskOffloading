-- DB migration for vehicle telemetry
CREATE TABLE IF NOT EXISTS vehicle_telemetry (
    id BIGSERIAL PRIMARY KEY,
    veh_id INTEGER,
    sim_time DOUBLE PRECISION,
    floc_hz DOUBLE PRECISION,
    tx_power_mw DOUBLE PRECISION,
    speed DOUBLE PRECISION,
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    mac TEXT,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_vehicle_telemetry_veh_time ON vehicle_telemetry (veh_id, sim_time);
CREATE INDEX IF NOT EXISTS idx_vehicle_telemetry_mac_time ON vehicle_telemetry (mac, sim_time);

-- Task metadata table
CREATE TABLE IF NOT EXISTS task_metadata (
    id BIGSERIAL PRIMARY KEY,
    task_id TEXT NOT NULL,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    task_size_bytes BIGINT,
    cpu_cycles BIGINT,
    qos_value DOUBLE PRECISION,
    created_time DOUBLE PRECISION,
    deadline_seconds DOUBLE PRECISION,
    received_time DOUBLE PRECISION,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_task_metadata_vehicle ON task_metadata (vehicle_id, created_time);
CREATE INDEX IF NOT EXISTS idx_task_metadata_task_id ON task_metadata (task_id);

-- Task completion/failure events
CREATE TABLE IF NOT EXISTS task_events (
    id BIGSERIAL PRIMARY KEY,
    task_id TEXT NOT NULL,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    event_type TEXT NOT NULL, -- 'COMPLETED' or 'FAILED'
    completion_time DOUBLE PRECISION,
    processing_time DOUBLE PRECISION,
    completed_on_time BOOLEAN,
    failure_reason TEXT,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_task_events_task_id ON task_events (task_id);
CREATE INDEX IF NOT EXISTS idx_task_events_vehicle ON task_events (vehicle_id, completion_time);

-- Vehicle resource status updates
CREATE TABLE IF NOT EXISTS vehicle_resources (
    id BIGSERIAL PRIMARY KEY,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    update_time DOUBLE PRECISION,
    cpu_total DOUBLE PRECISION,
    cpu_allocable DOUBLE PRECISION,
    cpu_available DOUBLE PRECISION,
    cpu_utilization DOUBLE PRECISION,
    mem_total DOUBLE PRECISION,
    mem_available DOUBLE PRECISION,
    mem_utilization DOUBLE PRECISION,
    queue_length INTEGER,
    processing_count INTEGER,
    tasks_generated INTEGER,
    tasks_completed_on_time INTEGER,
    tasks_completed_late INTEGER,
    tasks_failed INTEGER,
    tasks_rejected INTEGER,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_vehicle_resources_vehicle ON vehicle_resources (vehicle_id, update_time);
