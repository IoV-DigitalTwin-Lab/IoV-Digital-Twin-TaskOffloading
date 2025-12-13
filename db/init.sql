-- Unified vehicle status table (combines telemetry + resources)
CREATE TABLE IF NOT EXISTS vehicle_status (
    id BIGSERIAL PRIMARY KEY,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    update_time DOUBLE PRECISION,
    
    -- Position and movement
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    speed DOUBLE PRECISION,
    heading DOUBLE PRECISION,
    
    -- Resource information
    cpu_total DOUBLE PRECISION,
    cpu_allocable DOUBLE PRECISION,
    cpu_available DOUBLE PRECISION,
    cpu_utilization DOUBLE PRECISION,
    mem_total DOUBLE PRECISION,
    mem_available DOUBLE PRECISION,
    mem_utilization DOUBLE PRECISION,
    
    -- Task queue status
    queue_length INTEGER,
    processing_count INTEGER,
    
    -- Task statistics
    tasks_generated INTEGER,
    tasks_completed_on_time INTEGER,
    tasks_completed_late INTEGER,
    tasks_failed INTEGER,
    tasks_rejected INTEGER,
    avg_completion_time DOUBLE PRECISION,
    deadline_miss_ratio DOUBLE PRECISION,
    
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_vehicle_status_vehicle_time ON vehicle_status (vehicle_id, update_time);
CREATE INDEX IF NOT EXISTS idx_vehicle_status_rsu ON vehicle_status (rsu_id, update_time);

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


