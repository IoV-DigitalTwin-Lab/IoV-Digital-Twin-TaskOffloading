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
    acceleration DOUBLE PRECISION,
    
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
    -- Task profile fields
    task_type_name TEXT,
    task_type_id INTEGER DEFAULT 0,
    input_size_bytes BIGINT DEFAULT 0,
    output_size_bytes BIGINT DEFAULT 0,
    is_offloadable BOOLEAN DEFAULT TRUE,
    is_safety_critical BOOLEAN DEFAULT FALSE,
    priority_level INTEGER DEFAULT 2,
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

-- ============================================================================
-- RSU STATUS AND METADATA TABLES
-- ============================================================================

-- RSU real-time status (dynamic data - updated periodically)
CREATE TABLE IF NOT EXISTS rsu_status (
    id BIGSERIAL PRIMARY KEY,
    rsu_id VARCHAR(255) NOT NULL,
    update_time DOUBLE PRECISION NOT NULL,
    
    -- Edge server resources
    cpu_total DOUBLE PRECISION,          -- Total CPU capacity (GHz)
    cpu_allocable DOUBLE PRECISION,      -- Allocable CPU for tasks (GHz)
    cpu_available DOUBLE PRECISION,      -- Currently available CPU (GHz)
    cpu_utilization DOUBLE PRECISION,    -- CPU utilization (0.0-1.0)
    
    memory_total DOUBLE PRECISION,       -- Total memory (GB)
    memory_available DOUBLE PRECISION,   -- Available memory (GB)
    memory_utilization DOUBLE PRECISION, -- Memory utilization (0.0-1.0)
    
    -- Task processing status
    queue_length INTEGER,                -- Tasks in queue
    processing_count INTEGER,            -- Currently processing tasks
    max_concurrent_tasks INTEGER,        -- Maximum concurrent tasks
    
    -- Task statistics (cumulative)
    tasks_received INTEGER,              -- Total tasks received
    tasks_processed INTEGER,             -- Tasks successfully processed
    tasks_failed INTEGER,                -- Tasks that failed
    tasks_rejected INTEGER,              -- Tasks rejected (capacity)
    
    -- Performance metrics
    avg_processing_time DOUBLE PRECISION,  -- Average processing time (s)
    avg_queue_time DOUBLE PRECISION,       -- Average queue wait time (s)
    success_rate DOUBLE PRECISION,         -- Task success rate (0.0-1.0)
    
    -- Connected vehicles
    connected_vehicles_count INTEGER,    -- Number of connected vehicles
    
    payload JSONB,
    received_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_rsu_status_rsu_time ON rsu_status (rsu_id, update_time);
CREATE INDEX IF NOT EXISTS idx_rsu_status_received ON rsu_status (received_at);

-- RSU metadata (static/semi-static data - updated rarely)
CREATE TABLE IF NOT EXISTS rsu_metadata (
    id SERIAL PRIMARY KEY,
    rsu_id VARCHAR(255) UNIQUE NOT NULL,
    
    -- Physical location
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    pos_z DOUBLE PRECISION,
    coverage_radius DOUBLE PRECISION,    -- Coverage radius (meters)
    
    -- Hardware specifications
    cpu_capacity_ghz DOUBLE PRECISION,   -- Edge server CPU (GHz)
    memory_capacity_gb DOUBLE PRECISION, -- Edge server memory (GB)
    storage_capacity_gb DOUBLE PRECISION, -- Edge server storage (GB)
    
    -- Network specifications
    bandwidth_mbps DOUBLE PRECISION,     -- Network bandwidth (Mbps)
    max_vehicles INTEGER,                -- Max concurrent vehicles
    transmission_power_mw DOUBLE PRECISION, -- TX power (mW)
    
    -- Processing capabilities
    base_processing_delay_ms DOUBLE PRECISION, -- Base processing delay
    max_task_size_mb DOUBLE PRECISION,   -- Maximum task size
    supported_task_types TEXT[],         -- Array of supported task types
    
    -- Deployment info
    deployment_time TIMESTAMP,
    location_name VARCHAR(255),          -- e.g., "Intersection_A"
    deployment_type VARCHAR(100),        -- e.g., "Urban", "Highway"
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    payload JSONB
);

CREATE INDEX IF NOT EXISTS idx_rsu_metadata_location ON rsu_metadata (pos_x, pos_y);

-- Vehicle metadata (static/semi-static data)
CREATE TABLE IF NOT EXISTS vehicle_metadata (
    id SERIAL PRIMARY KEY,
    vehicle_id VARCHAR(255) UNIQUE NOT NULL,
    
    -- Hardware specifications
    cpu_capacity_ghz DOUBLE PRECISION,   -- Total CPU capacity
    memory_capacity_mb DOUBLE PRECISION, -- Total memory
    storage_capacity_gb DOUBLE PRECISION, -- Storage capacity
    battery_capacity_kwh DOUBLE PRECISION, -- Battery capacity (for EVs)
    
    -- Communication capabilities
    transmission_power_mw DOUBLE PRECISION, -- TX power
    max_communication_range_m DOUBLE PRECISION, -- Max range
    supported_protocols TEXT[],          -- Communication protocols
    
    -- Vehicle type and capabilities
    vehicle_type VARCHAR(100),           -- e.g., "Sedan", "Truck", "Bus"
    service_vehicle BOOLEAN DEFAULT false, -- Can process tasks for others
    offloading_enabled BOOLEAN DEFAULT true, -- Can offload tasks
    max_concurrent_tasks INTEGER,        -- Max concurrent local tasks
    max_queue_size INTEGER,              -- Max task queue size
    
    -- Mobility pattern (optional)
    typical_route TEXT,                  -- Description of route
    average_speed_kmh DOUBLE PRECISION,  -- Average speed
    
    -- Registration info
    first_seen_time DOUBLE PRECISION,    -- Simulation time first seen
    last_seen_time DOUBLE PRECISION,     -- Simulation time last seen
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    payload JSONB
);

CREATE INDEX IF NOT EXISTS idx_vehicle_metadata_type ON vehicle_metadata (vehicle_type);
CREATE INDEX IF NOT EXISTS idx_vehicle_metadata_service ON vehicle_metadata (service_vehicle);

-- ============================================================================
-- MIGRATION: Add task profile columns to task_metadata (idempotent)
-- ============================================================================
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS task_type_name TEXT;
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS task_type_id INTEGER DEFAULT 0;
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS input_size_bytes BIGINT DEFAULT 0;
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS output_size_bytes BIGINT DEFAULT 0;
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS is_offloadable BOOLEAN DEFAULT TRUE;
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS is_safety_critical BOOLEAN DEFAULT FALSE;
ALTER TABLE task_metadata ADD COLUMN IF NOT EXISTS priority_level INTEGER DEFAULT 2;
