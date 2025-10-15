# IoV Telemetry Ingest Service

This folder contains a small FastAPI-based ingestion service to receive vehicle telemetry
from the simulation RSU and store it in PostgreSQL.

Requirements
- Python 3.9+
- PostgreSQL

Setup
1. Create the database and user (example):

```bash
sudo -u postgres psql -c "CREATE USER iov_user WITH PASSWORD 'iov_pass';"
sudo -u postgres psql -c "CREATE DATABASE iov_db OWNER iov_user;"
```

2. Apply DB migration:

```bash
psql -U iov_user -d iov_db -f db/init.sql
```

3. Create a virtualenv and install dependencies:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r ingest/requirements.txt
```

Run

```bash
# default listens on 0.0.0.0:8000 and POST /ingest
uvicorn ingest.main:app --host 0.0.0.0 --port 8000
```

Test

```bash
curl -X POST http://localhost:8000/ingest -H "Content-Type: application/json" -d \
  '{"VehID":0,"Time":11.0,"FlocHz":3000000000,"TxPower_mW":100,"Speed":15.31,"PosX":124.48,"PosY":26.60,"MAC":"22"}'
```

Next steps
- Add authentication or a secret token if exposing the ingestion endpoint to untrusted networks.
- Implement retry logic and backpressure handling on the RSU side.
- Optionally add metrics and logging to a file or monitoring system.
