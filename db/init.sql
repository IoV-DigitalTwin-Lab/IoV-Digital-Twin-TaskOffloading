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
