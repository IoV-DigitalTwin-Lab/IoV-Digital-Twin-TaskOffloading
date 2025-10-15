-- Database schema for IoV Digital Twin Task Offloading System
-- Three separate tables for different node types: Task Vehicles, Service Vehicles, and RSUs

-- ========================================
-- Task Vehicle (TV) Telemetry Table
-- ========================================
CREATE TABLE IF NOT EXISTS task_vehicle_telemetry (
    id BIGSERIAL PRIMARY KEY,
    node_id INTEGER NOT NULL,
    sim_time DOUBLE PRECISION,
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    velocity DOUBLE PRECISION,
    mac TEXT,
    local_cpu_hz DOUBLE PRECISION,
    pending_tasks INTEGER,
    energy_j DOUBLE PRECISION,
    tx_power_w DOUBLE PRECISION,
    svs_in_range INTEGER,
    rsus_in_range INTEGER,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_tv_node_time ON task_vehicle_telemetry (node_id, sim_time);
CREATE INDEX IF NOT EXISTS idx_tv_received_at ON task_vehicle_telemetry (received_at);

-- ========================================
-- Service Vehicle (SV) Telemetry Table
-- ========================================
CREATE TABLE IF NOT EXISTS service_vehicle_telemetry (
    id BIGSERIAL PRIMARY KEY,
    node_id INTEGER NOT NULL,
    sim_time DOUBLE PRECISION,
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    velocity DOUBLE PRECISION,
    mac TEXT,
    service_cpu_hz DOUBLE PRECISION,
    available_cpu_hz DOUBLE PRECISION,
    cpu_util DOUBLE PRECISION,
    queue_size INTEGER,
    energy_j DOUBLE PRECISION,
    tx_power_w DOUBLE PRECISION,
    tvs_in_range INTEGER,
    rsus_in_range INTEGER,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_sv_node_time ON service_vehicle_telemetry (node_id, sim_time);
CREATE INDEX IF NOT EXISTS idx_sv_received_at ON service_vehicle_telemetry (received_at);

-- ========================================
-- RSU Telemetry Table
-- ========================================
CREATE TABLE IF NOT EXISTS rsu_telemetry (
    id BIGSERIAL PRIMARY KEY,
    node_id INTEGER NOT NULL,
    sim_time DOUBLE PRECISION,
    pos_x DOUBLE PRECISION,
    pos_y DOUBLE PRECISION,
    mac TEXT,
    cpu_freq_hz DOUBLE PRECISION,
    cpu_util DOUBLE PRECISION,
    bw_util DOUBLE PRECISION,
    queue_size INTEGER,
    energy_j DOUBLE PRECISION,
    tx_power_w DOUBLE PRECISION,
    vehicles_in_range INTEGER,
    payload JSONB,
    received_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_rsu_node_time ON rsu_telemetry (node_id, sim_time);
CREATE INDEX IF NOT EXISTS idx_rsu_received_at ON rsu_telemetry (received_at);

-- ========================================
-- Grant permissions to iov_user
-- ========================================
GRANT ALL PRIVILEGES ON TABLE task_vehicle_telemetry TO iov_user;
GRANT ALL PRIVILEGES ON TABLE service_vehicle_telemetry TO iov_user;
GRANT ALL PRIVILEGES ON TABLE rsu_telemetry TO iov_user;

GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO iov_user;
