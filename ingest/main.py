from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Any, Dict, Optional
import asyncpg
import json
import os
import logging
import sys

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger("ingest")
logger.setLevel(logging.DEBUG)

def _prt(*args, **kwargs):
    print(*args, **kwargs)
    try:
        sys.stdout.flush()
    except Exception:
        pass

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
        # pydantic v1 used `allow_population_by_field_name`; in v2 it was renamed to
        # `validate_by_name`. We'll set the appropriate attribute at runtime below so
        # the code works with either pydantic v1 or v2 and avoids the deprecation
        # warning you saw.
        allow_population_by_field_name = True
        extra = "allow"


# Try to set the newer pydantic v2 config attribute if available, otherwise
# keep the v1 attribute. This avoids the runtime deprecation warning.
try:
    import pydantic
    ver = tuple(int(x) for x in pydantic.__version__.split(".")[:2])
except Exception:
    ver = (0, 0)

if ver[0] >= 2:
    # pydantic v2: use validate_by_name
    try:
        Telemetry.Config.validate_by_name = True
    except Exception:
        # if attribute doesn't exist for some reason, fall back silently
        pass
else:
    # pydantic v1: keep allow_population_by_field_name (already set above)
    try:
        Telemetry.Config.allow_population_by_field_name = True
    except Exception:
        pass


@app.on_event("startup")
async def startup_event():
    global db_pool
    logger.info(f"Connecting to DB: {DATABASE_URL}")
    _prt(f"[ingest] startup: Connecting to DB: {DATABASE_URL}")
    db_pool = await asyncpg.create_pool(DATABASE_URL, min_size=1, max_size=10)
    logger.info("DB pool created")
    _prt("[ingest] startup: DB pool created")


@app.on_event("shutdown")
async def shutdown_event():
    global db_pool
    if db_pool is not None:
        await db_pool.close()
        logger.info("DB pool closed")
        _prt("[ingest] shutdown: DB pool closed")


@app.post(INGEST_PATH)
async def ingest(telemetry: Telemetry):
    # Convert to dict (includes extra fields in telemetry.__dict__? use .dict(by_alias=True, exclude_none=True))
    payload = telemetry.dict(by_alias=True, exclude_none=True)
    _prt("[ingest] received payload:", payload)
    logger.debug("Received payload: %s", payload)

    veh_id = payload.get("VehID")
    sim_time = payload.get("Time")
    floc_hz = payload.get("FlocHz")
    tx_power = payload.get("TxPower_mW")
    speed = payload.get("Speed")
    pos_x = payload.get("PosX")
    pos_y = payload.get("PosY")
    mac = payload.get("MAC")

    # Fallbacks: if pool missing
    if db_pool is None:
        raise HTTPException(status_code=503, detail="DB pool not initialized")

    query = """
    INSERT INTO vehicle_telemetry (veh_id, sim_time, floc_hz, tx_power_mw, speed, pos_x, pos_y, mac, payload)
    VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)
    """

    async with db_pool.acquire() as conn:
        try:
            # asyncpg may expect a str for the JSONB parameter in some environments,
            # so serialize the payload to JSON here to be safe.
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
            _prt("[ingest] inserted telemetry for veh", veh_id, "time", sim_time)
            logger.debug("Inserted telemetry for veh %s time %s", veh_id, sim_time)
        except Exception as e:
            logger.exception("Failed to insert telemetry")
            _prt("[ingest] ERROR inserting telemetry:", e)
            raise HTTPException(status_code=500, detail=str(e))

    return {"ok": True}


@app.get("/health")
async def health():
    return {"status": "ok"}
