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

-- Offloading request table (Digital Twin tracking)
CREATE TABLE IF NOT EXISTS offloading_requests (
    id BIGSERIAL PRIMARY KEY,
    task_id TEXT NOT NULL,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    request_time DOUBLE PRECISION,
    
    -- Task characteristics
    task_size_bytes BIGINT,
    cpu_cycles BIGINT,
    deadline_seconds DOUBLE PRECISION,
    qos_value DOUBLE PRECISION,
    
    -- Vehicle state at request time
    vehicle_cpu_available DOUBLE PRECISION,
    vehicle_cpu_utilization DOUBLE PRECISION,
    vehicle_mem_available DOUBLE PRECISION,
    vehicle_queue_length INTEGER,
    vehicle_processing_count INTEGER,
    
    -- Vehicle location
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    speed DOUBLE PRECISION,
    
    -- Local decision recommendation
    local_decision TEXT,
    
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_offloading_requests_task_id ON offloading_requests (task_id);
CREATE INDEX IF NOT EXISTS idx_offloading_requests_vehicle ON offloading_requests (vehicle_id, request_time);
CREATE INDEX IF NOT EXISTS idx_offloading_requests_rsu ON offloading_requests (rsu_id, request_time);

-- Offloading decision table (ML model outputs)
CREATE TABLE IF NOT EXISTS offloading_decisions (
    id BIGSERIAL PRIMARY KEY,
    task_id TEXT NOT NULL,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    decision_time DOUBLE PRECISION,
    
    -- Decision details
    decision_type TEXT NOT NULL, -- 'LOCAL', 'RSU', 'SERVICE_VEHICLE', 'REJECT'
    target_service_vehicle_id TEXT,
    confidence_score DOUBLE PRECISION,
    estimated_completion_time DOUBLE PRECISION,
    decision_reason TEXT,
    
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_offloading_decisions_task_id ON offloading_decisions (task_id);
CREATE INDEX IF NOT EXISTS idx_offloading_decisions_vehicle ON offloading_decisions (vehicle_id, decision_time);
CREATE INDEX IF NOT EXISTS idx_offloading_decisions_type ON offloading_decisions (decision_type, decision_time);

-- Task offloading event table (lifecycle tracking)
CREATE TABLE IF NOT EXISTS task_offloading_events (
    id BIGSERIAL PRIMARY KEY,
    task_id TEXT NOT NULL,
    event_type TEXT NOT NULL, -- 'REQUEST_SENT', 'DECISION_RECEIVED', 'OFFLOAD_SENT', 'PROCESSING_STARTED', 'RESULT_RECEIVED', 'TIMEOUT', etc.
    event_time DOUBLE PRECISION,
    source_entity_id TEXT,
    target_entity_id TEXT,
    rsu_id INTEGER,
    event_details JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_task_offloading_events_task_id ON task_offloading_events (task_id, event_time);
CREATE INDEX IF NOT EXISTS idx_task_offloading_events_type ON task_offloading_events (event_type, event_time);
CREATE INDEX IF NOT EXISTS idx_task_offloading_events_source ON task_offloading_events (source_entity_id, event_time);

-- Task completion tracking for offloaded tasks
CREATE TABLE IF NOT EXISTS offloaded_task_completions (
    id BIGSERIAL PRIMARY KEY,
    task_id TEXT NOT NULL,
    vehicle_id TEXT NOT NULL,
    rsu_id INTEGER,
    
    -- Decision details
    decision_type TEXT NOT NULL, -- 'LOCAL', 'RSU', 'SERVICE_VEHICLE'
    processor_id TEXT, -- ID of the entity that processed the task
    
    -- Timing information
    request_time DOUBLE PRECISION, -- When offloading request was sent
    decision_time DOUBLE PRECISION, -- When RSU made decision
    start_time DOUBLE PRECISION, -- When processing started
    completion_time DOUBLE PRECISION, -- When task completed
    
    -- Latency metrics
    decision_latency DOUBLE PRECISION, -- decision_time - request_time
    processing_latency DOUBLE PRECISION, -- completion_time - start_time
    total_latency DOUBLE PRECISION, -- completion_time - request_time
    
    -- Completion status
    success BOOLEAN, -- Task completed successfully
    completed_on_time BOOLEAN, -- Met deadline
    deadline_seconds DOUBLE PRECISION, -- Original deadline
    
    -- Task characteristics
    task_size_bytes BIGINT,
    cpu_cycles BIGINT,
    qos_value DOUBLE PRECISION,
    
    -- Result info
    result_data TEXT,
    failure_reason TEXT,
    
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_offloaded_completions_task_id ON offloaded_task_completions (task_id);
CREATE INDEX IF NOT EXISTS idx_offloaded_completions_vehicle ON offloaded_task_completions (vehicle_id, completion_time);
CREATE INDEX IF NOT EXISTS idx_offloaded_completions_decision ON offloaded_task_completions (decision_type, success);
CREATE INDEX IF NOT EXISTS idx_offloaded_completions_rsu ON offloaded_task_completions (rsu_id, completion_time);
