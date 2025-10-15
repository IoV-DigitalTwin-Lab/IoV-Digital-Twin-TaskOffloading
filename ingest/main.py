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


@app.get("/test")
async def test_insert():
    """Test endpoint to verify DB connection"""
    if db_pool is None:
        raise HTTPException(status_code=503, detail="DB pool not initialized")
    
    test_data = {
        "VehID": 999,
        "Time": 0.0,
        "Speed": 0.0,
        "PosX": 0.0,
        "PosY": 0.0,
        "MAC": "TEST"
    }
    
    query = """
    INSERT INTO vehicle_telemetry (veh_id, sim_time, speed, pos_x, pos_y, mac, payload)
    VALUES ($1,$2,$3,$4,$5,$6,$7)
    """
    
    async with db_pool.acquire() as conn:
        try:
            await conn.execute(query,
                               test_data["VehID"],
                               test_data["Time"],
                               test_data["Speed"],
                               test_data["PosX"],
                               test_data["PosY"],
                               test_data["MAC"],
                               json.dumps(test_data))
            return {"ok": True, "message": "Test insert successful"}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))