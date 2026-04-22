d#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

REDIS_HOST="${REDIS_HOST:-127.0.0.1}"
REDIS_PORT="${REDIS_PORT:-6379}"
REDIS_DB="${REDIS_DB:-3}"
WITH_RUN_ID="${WITH_RUN_ID:-DT-Secondary-Shadowing-With}"
WITHOUT_RUN_ID="${WITHOUT_RUN_ID:-DT-Secondary-Shadowing-Without}"
RUN_NUMBER="${RUN_NUMBER:-0}"
SIM_TIME_LIMIT="${SIM_TIME_LIMIT:-7200s}"
FAST_MODE="${FAST_MODE:-1}"

WITH_CONFIG="${WITH_CONFIG:-VehicleShadowingExp_With}"
WITHOUT_CONFIG="${WITHOUT_CONFIG:-VehicleShadowingExp_Without}"

PLOT_ARGS=("$@")
SIM_OVERRIDES=("--sim-time-limit=${SIM_TIME_LIMIT}")

# Fast mode disables heavy result recording while keeping Redis stream export.
if [[ "$FAST_MODE" == "1" ]]; then
    SIM_OVERRIDES+=("--**.vector-recording=false")
    SIM_OVERRIDES+=("--**.scalar-recording=false")
    SIM_OVERRIDES+=("--cmdenv-status-frequency=10s")
fi

if command -v redis-cli >/dev/null 2>&1; then
    echo "[shadowing-exp] Redis keys before run:"
    redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB" KEYS "dt2:q:${WITH_RUN_ID}:*" | wc -l || true
    redis-cli -h "$REDIS_HOST" -p "$REDIS_PORT" -n "$REDIS_DB" KEYS "dt2:q:${WITHOUT_RUN_ID}:*" | wc -l || true
fi

echo "[shadowing-exp] sim-time-limit=$SIM_TIME_LIMIT fast-mode=$FAST_MODE"

echo "[shadowing-exp] Running WITH vehicle shadowing config: $WITH_CONFIG"
export REDIS_DB
DT2_CONFIG="$WITH_CONFIG" \
DT2_RUN="$RUN_NUMBER" \
DT2_TRACI_PORT="10000" \
./run_secondary_dt.sh "${SIM_OVERRIDES[@]}"

echo "[shadowing-exp] Running WITHOUT vehicle shadowing config: $WITHOUT_CONFIG"
DT2_CONFIG="$WITHOUT_CONFIG" \
DT2_RUN="$RUN_NUMBER" \
DT2_TRACI_PORT="10001" \
./run_secondary_dt.sh "${SIM_OVERRIDES[@]}"

echo "[shadowing-exp] Generating plots..."
python3 ./plot_shadowing_experiment.py \
    --redis-host "$REDIS_HOST" \
    --redis-port "$REDIS_PORT" \
    --redis-db "$REDIS_DB" \
    --with-run-id "$WITH_RUN_ID" \
    --without-run-id "$WITHOUT_RUN_ID" \
    "${PLOT_ARGS[@]}"

echo "[shadowing-exp] Complete. Check plots/ directory."
