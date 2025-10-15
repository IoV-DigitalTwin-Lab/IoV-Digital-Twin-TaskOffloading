from fastapi import FastAPI, HTTPException, Request
from pydantic import BaseModel, Field
from typing import Any, Dict, Optional
import asyncpg
import json
import os
import logging
import sys
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler('ingest_server.log')
    ]
)
logger = logging.getLogger("ingest")

def _prt(*args, **kwargs):
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
    print(f"[{timestamp}]", *args, **kwargs, flush=True)

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://iov_user:iov_pass@localhost/iov_db")
INGEST_PATH = os.getenv("INGEST_PATH", "/ingest")

app = FastAPI(title="IoV Telemetry Ingest")

db_pool: Optional[asyncpg.pool.Pool] = None

class Telemetry(BaseModel):
    VehID: Optional[int] = Field(None, alias="VehID")
    Time: Optional[float] = Field(None, alias="Time")
    FlocHz: Optional[float] = Field(None, alias="FlocHz")
    TxPower_mW: Optional[float] = Field(None, alias="TxPower_mW")
    Speed: Optional[float] = Field(None, alias="Speed")
    PosX: Optional[float] = Field(None, alias="PosX")
    PosY: Optional[float] = Field(None, alias="PosY")
    MAC: Optional[str] = Field(None, alias="MAC")

    class Config:
        populate_by_name = True
        extra = "allow"


@app.on_event("startup")
async def startup_event():
    global db_pool
    _prt("="*60)
    _prt("INGEST SERVER STARTUP")
    _prt("="*60)
    _prt(f"Database URL: {DATABASE_URL}")
    _prt(f"Ingest path: {INGEST_PATH}")
    
    logger.info(f"Connecting to DB: {DATABASE_URL}")
    
    try:
        db_pool = await asyncpg.create_pool(DATABASE_URL, min_size=1, max_size=10)
        _prt("✓ DB pool created successfully")
        logger.info("DB pool created")
        
        # Test DB connection
        async with db_pool.acquire() as conn:
            version = await conn.fetchval('SELECT version()')
            _prt(f"✓ DB connection test successful")
            _prt(f"  PostgreSQL version: {version[:50]}...")
            
    except Exception as e:
        _prt(f"✗ FAILED to create DB pool: {e}")
        logger.error(f"Failed to create DB pool: {e}")
        _prt("⚠ Server will continue but DB operations will fail!")
        # Don't raise - let server start for debugging
    
    _prt("="*60)
    _prt(f"✓ Server ready on {INGEST_PATH}")
    _prt("="*60)


@app.on_event("shutdown")
async def shutdown_event():
    global db_pool
    _prt("="*60)
    _prt("INGEST SERVER SHUTDOWN")
    _prt("="*60)
    
    if db_pool is not None:
        await db_pool.close()
        _prt("✓ DB pool closed")
        logger.info("DB pool closed")


@app.post(INGEST_PATH)
async def ingest(request: Request, telemetry: Telemetry):
    _prt("\n" + "="*60)
    _prt("NEW REQUEST RECEIVED")
    _prt("="*60)
    
    # Log request metadata
    _prt(f"Client: {request.client.host}:{request.client.port}")
    _prt(f"Headers: {dict(request.headers)}")
    
    # Log raw request body
    body = await request.body()
    body_str = body.decode('utf-8', errors='ignore')
    _prt(f"Raw body ({len(body)} bytes): {body_str}")
    
    # Convert to dict
    payload = telemetry.dict(by_alias=True, exclude_none=True)
    _prt(f"Parsed payload: {json.dumps(payload, indent=2)}")
    logger.debug("Received payload: %s", payload)

    veh_id = payload.get("VehID")
    sim_time = payload.get("Time")
    floc_hz = payload.get("FlocHz")
    tx_power = payload.get("TxPower_mW")
    speed = payload.get("Speed")
    pos_x = payload.get("PosX")
    pos_y = payload.get("PosY")
    mac = payload.get("MAC")

    _prt(f"Extracted fields:")
    _prt(f"  VehID: {veh_id}")
    _prt(f"  Time: {sim_time}")
    _prt(f"  Speed: {speed}")
    _prt(f"  Position: ({pos_x}, {pos_y})")
    _prt(f"  MAC: {mac}")

    if db_pool is None:
        _prt("✗ ERROR: DB pool not initialized!")
        logger.error("DB pool not initialized")
        raise HTTPException(status_code=503, detail="DB pool not initialized")

    query = """
    INSERT INTO vehicle_telemetry (veh_id, sim_time, floc_hz, tx_power_mw, speed, pos_x, pos_y, mac, payload)
    VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)
    """

    _prt("Attempting DB insert...")
    
    async with db_pool.acquire() as conn:
        try:
            payload_json = json.dumps(payload)
            await conn.execute(query,
                               veh_id,
                               sim_time,
                               floc_hz,
                               tx_power,
                               speed,
                               pos_x,
                               pos_y,
                               mac,
                               payload_json)
            
            _prt(f"✓ SUCCESS: Inserted telemetry for vehicle {veh_id} at time {sim_time}")
            logger.info(f"Inserted telemetry for veh {veh_id} time {sim_time}")
            
        except Exception as e:
            _prt(f"✗ ERROR inserting telemetry: {e}")
            logger.exception("Failed to insert telemetry")
            raise HTTPException(status_code=500, detail=str(e))

    _prt("="*60 + "\n")
    return {"ok": True, "veh_id": veh_id, "sim_time": sim_time}


@app.get("/health")
async def health():
    _prt("[health] Health check called")
    db_status = "connected" if db_pool is not None else "disconnected"
    return {
        "status": "ok", 
        "endpoint": INGEST_PATH,
        "database": db_status
    }


@app.get("/")
async def root():
    return {
        "message": "IoV Telemetry Ingest API",
        "version": "1.0",
        "ingest_endpoint": INGEST_PATH,
        "health_endpoint": "/health"
    }


# ========================================
# Digital Twin Update Models
# ========================================

class TaskVehicleUpdate(BaseModel):
    """Task Vehicle (TV) Digital Twin Update"""
    Type: Optional[str] = Field(None, alias="Type")
    NodeID: Optional[int] = Field(None, alias="NodeID")
    Time: Optional[float] = Field(None, alias="Time")
    PosX: Optional[float] = Field(None, alias="PosX")
    PosY: Optional[float] = Field(None, alias="PosY")
    Velocity: Optional[float] = Field(None, alias="Velocity")
    MAC: Optional[str] = Field(None, alias="MAC")
    LocalCPUHz: Optional[float] = Field(None, alias="LocalCPUHz")
    PendingTasks: Optional[int] = Field(None, alias="PendingTasks")
    EnergyJ: Optional[float] = Field(None, alias="EnergyJ")
    TxPowerW: Optional[float] = Field(None, alias="TxPowerW")
    SVsInRange: Optional[int] = Field(None, alias="SVsInRange")
    RSUsInRange: Optional[int] = Field(None, alias="RSUsInRange")
    
    class Config:
        populate_by_name = True
        extra = "allow"


class ServiceVehicleUpdate(BaseModel):
    """Service Vehicle (SV) Digital Twin Update"""
    Type: Optional[str] = Field(None, alias="Type")
    NodeID: Optional[int] = Field(None, alias="NodeID")
    Time: Optional[float] = Field(None, alias="Time")
    PosX: Optional[float] = Field(None, alias="PosX")
    PosY: Optional[float] = Field(None, alias="PosY")
    Velocity: Optional[float] = Field(None, alias="Velocity")
    MAC: Optional[str] = Field(None, alias="MAC")
    ServiceCPUHz: Optional[float] = Field(None, alias="ServiceCPUHz")
    AvailableCPUHz: Optional[float] = Field(None, alias="AvailableCPUHz")
    CPUUtil: Optional[float] = Field(None, alias="CPUUtil")
    QueueSize: Optional[int] = Field(None, alias="QueueSize")
    EnergyJ: Optional[float] = Field(None, alias="EnergyJ")
    TxPowerW: Optional[float] = Field(None, alias="TxPowerW")
    TVsInRange: Optional[int] = Field(None, alias="TVsInRange")
    RSUsInRange: Optional[int] = Field(None, alias="RSUsInRange")
    
    class Config:
        populate_by_name = True
        extra = "allow"


class RSUUpdate(BaseModel):
    """RSU Digital Twin Update"""
    Type: Optional[str] = Field(None, alias="Type")
    NodeID: Optional[int] = Field(None, alias="NodeID")
    Time: Optional[float] = Field(None, alias="Time")
    PosX: Optional[float] = Field(None, alias="PosX")
    PosY: Optional[float] = Field(None, alias="PosY")
    MAC: Optional[str] = Field(None, alias="MAC")
    CPUFreqHz: Optional[float] = Field(None, alias="CPUFreqHz")
    CPUUtil: Optional[float] = Field(None, alias="CPUUtil")
    BWUtil: Optional[float] = Field(None, alias="BWUtil")
    QueueSize: Optional[int] = Field(None, alias="QueueSize")
    EnergyJ: Optional[float] = Field(None, alias="EnergyJ")
    TxPowerW: Optional[float] = Field(None, alias="TxPowerW")
    VehiclesInRange: Optional[int] = Field(None, alias="VehiclesInRange")
    
    class Config:
        populate_by_name = True
        extra = "allow"


# ========================================
# Digital Twin Update Endpoints
# ========================================

@app.post("/dt_update/tv")
async def ingest_tv_update(request: Request, update: TaskVehicleUpdate):
    """Endpoint for Task Vehicle (TV) Digital Twin updates"""
    _prt("\n" + "="*60)
    _prt("TASK VEHICLE (TV) DT_UPDATE RECEIVED")
    _prt("="*60)
    
    payload = update.dict(by_alias=True, exclude_none=True)
    _prt(f"TV Update: {json.dumps(payload, indent=2)}")
    
    if db_pool is None:
        raise HTTPException(status_code=503, detail="DB pool not initialized")
    
    query = """
    INSERT INTO task_vehicle_telemetry 
    (node_id, sim_time, pos_x, pos_y, velocity, mac, local_cpu_hz, pending_tasks, 
     energy_j, tx_power_w, svs_in_range, rsus_in_range, payload)
    VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13)
    """
    
    async with db_pool.acquire() as conn:
        try:
            await conn.execute(query,
                               payload.get("NodeID"),
                               payload.get("Time"),
                               payload.get("PosX"),
                               payload.get("PosY"),
                               payload.get("Velocity"),
                               payload.get("MAC"),
                               payload.get("LocalCPUHz"),
                               payload.get("PendingTasks"),
                               payload.get("EnergyJ"),
                               payload.get("TxPowerW"),
                               payload.get("SVsInRange"),
                               payload.get("RSUsInRange"),
                               json.dumps(payload))
            
            _prt(f"✓ SUCCESS: TV[{payload.get('NodeID')}] DT update stored")
            
        except Exception as e:
            _prt(f"✗ ERROR: {e}")
            logger.exception("Failed to insert TV update")
            raise HTTPException(status_code=500, detail=str(e))
    
    _prt("="*60 + "\n")
    return {"ok": True, "type": "TV", "node_id": payload.get("NodeID")}


@app.post("/dt_update/sv")
async def ingest_sv_update(request: Request, update: ServiceVehicleUpdate):
    """Endpoint for Service Vehicle (SV) Digital Twin updates"""
    _prt("\n" + "="*60)
    _prt("SERVICE VEHICLE (SV) DT_UPDATE RECEIVED")
    _prt("="*60)
    
    payload = update.dict(by_alias=True, exclude_none=True)
    _prt(f"SV Update: {json.dumps(payload, indent=2)}")
    
    if db_pool is None:
        raise HTTPException(status_code=503, detail="DB pool not initialized")
    
    query = """
    INSERT INTO service_vehicle_telemetry 
    (node_id, sim_time, pos_x, pos_y, velocity, mac, service_cpu_hz, available_cpu_hz, 
     cpu_util, queue_size, energy_j, tx_power_w, tvs_in_range, rsus_in_range, payload)
    VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15)
    """
    
    async with db_pool.acquire() as conn:
        try:
            await conn.execute(query,
                               payload.get("NodeID"),
                               payload.get("Time"),
                               payload.get("PosX"),
                               payload.get("PosY"),
                               payload.get("Velocity"),
                               payload.get("MAC"),
                               payload.get("ServiceCPUHz"),
                               payload.get("AvailableCPUHz"),
                               payload.get("CPUUtil"),
                               payload.get("QueueSize"),
                               payload.get("EnergyJ"),
                               payload.get("TxPowerW"),
                               payload.get("TVsInRange"),
                               payload.get("RSUsInRange"),
                               json.dumps(payload))
            
            _prt(f"✓ SUCCESS: SV[{payload.get('NodeID')}] DT update stored")
            
        except Exception as e:
            _prt(f"✗ ERROR: {e}")
            logger.exception("Failed to insert SV update")
            raise HTTPException(status_code=500, detail=str(e))
    
    _prt("="*60 + "\n")
    return {"ok": True, "type": "SV", "node_id": payload.get("NodeID")}


@app.post("/dt_update/rsu")
async def ingest_rsu_update(request: Request, update: RSUUpdate):
    """Endpoint for RSU Digital Twin updates"""
    _prt("\n" + "="*60)
    _prt("RSU DT_UPDATE RECEIVED")
    _prt("="*60)
    
    payload = update.dict(by_alias=True, exclude_none=True)
    _prt(f"RSU Update: {json.dumps(payload, indent=2)}")
    
    if db_pool is None:
        raise HTTPException(status_code=503, detail="DB pool not initialized")
    
    query = """
    INSERT INTO rsu_telemetry 
    (node_id, sim_time, pos_x, pos_y, mac, cpu_freq_hz, cpu_util, bw_util, 
     queue_size, energy_j, tx_power_w, vehicles_in_range, payload)
    VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13)
    """
    
    async with db_pool.acquire() as conn:
        try:
            await conn.execute(query,
                               payload.get("NodeID"),
                               payload.get("Time"),
                               payload.get("PosX"),
                               payload.get("PosY"),
                               payload.get("MAC"),
                               payload.get("CPUFreqHz"),
                               payload.get("CPUUtil"),
                               payload.get("BWUtil"),
                               payload.get("QueueSize"),
                               payload.get("EnergyJ"),
                               payload.get("TxPowerW"),
                               payload.get("VehiclesInRange"),
                               json.dumps(payload))
            
            _prt(f"✓ SUCCESS: RSU[{payload.get('NodeID')}] DT update stored")
            
        except Exception as e:
            _prt(f"✗ ERROR: {e}")
            logger.exception("Failed to insert RSU update")
            raise HTTPException(status_code=500, detail=str(e))
    
    _prt("="*60 + "\n")
    return {"ok": True, "type": "RSU", "node_id": payload.get("NodeID")}