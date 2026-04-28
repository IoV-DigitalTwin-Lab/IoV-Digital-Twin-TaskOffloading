#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Keep launcher display and controller runtime default aligned (100 ms).
export DT2_POLL_INTERVAL_S="${DT2_POLL_INTERVAL_S:-0.1}"
# Secondary run should not write primary vehicle DT heartbeat snapshots.
export DISABLE_DIRECT_VEHICLE_REDIS="${DISABLE_DIRECT_VEHICLE_REDIS:-1}"

CTRL_PID=""
SIM_PID=""
SHUTTING_DOWN=0
cleanup() {
    if [[ "$SHUTTING_DOWN" -eq 1 ]]; then
        return 0
    fi
    SHUTTING_DOWN=1

    if [[ -n "$SIM_PID" ]] && kill -0 "$SIM_PID" 2>/dev/null; then
        echo "[run_secondary_dt] Stopping secondary simulation (PID $SIM_PID)..."
        kill -TERM "$SIM_PID" 2>/dev/null || true

        local elapsed=0
        local timeout="${DT2_SHUTDOWN_TIMEOUT_S:-15}"
        while kill -0 "$SIM_PID" 2>/dev/null && (( elapsed < timeout )); do
            sleep 1
            ((elapsed++)) || true
        done

        if kill -0 "$SIM_PID" 2>/dev/null; then
            echo "[run_secondary_dt] Secondary simulation did not stop after ${timeout}s; sending SIGTERM..."
            kill -TERM "$SIM_PID" 2>/dev/null || true
            sleep 2
        fi

        if kill -0 "$SIM_PID" 2>/dev/null; then
            echo "[run_secondary_dt] Secondary simulation still alive; force killing."
            kill -KILL "$SIM_PID" 2>/dev/null || true
        fi

        wait "$SIM_PID" 2>/dev/null || true
        SIM_PID=""
    fi

    if [[ -n "$CTRL_PID" ]] && kill -0 "$CTRL_PID" 2>/dev/null; then
        echo "[run_secondary_dt] Stopping external controller (PID $CTRL_PID)..."
        kill -TERM "$CTRL_PID" 2>/dev/null || true
        wait "$CTRL_PID" 2>/dev/null || true
        CTRL_PID=""
    fi
}

on_signal() {
    cleanup
    trap - EXIT INT TERM
    exit 130
}

trap cleanup EXIT
trap on_signal INT TERM

ensure_sim_binary_fresh() {
    local bin="$SCRIPT_DIR/IoV-Digital-Twin-TaskOffloading"
    local stale_src=""
    if [[ -x "$bin" ]]; then
        stale_src="$(find "$SCRIPT_DIR" -maxdepth 1 \( -name '*.cc' -o -name '*.h' -o -name '*.msg' -o -name '*.ned' \) -newer "$bin" -print -quit)"
    fi

    if [[ ! -x "$bin" || -n "$stale_src" ]]; then
        echo "[run_secondary_dt] Rebuilding simulation binary (source newer than executable)..."
        # shellcheck disable=SC1091
        set +u
        source /opt/omnet/omnetpp-6.1/setenv
        set -u
        make -C "$SCRIPT_DIR" -j"${DT2_BUILD_JOBS:-4}" MODE=release
    fi
}

# Build C++ external controller.
CTRL_BIN="$SCRIPT_DIR/external_controller_cpp"
CTRL_SRC="$SCRIPT_DIR/external_controller.cpp"
build_cpp_controller() {
    local cxx="${CXX:-g++}"
    local cflags="-std=c++17 -O2"
    local libs
    local fallback_inc="-I/home/mihiraja/.local/include"
    local fallback_lib="-L/home/mihiraja/.local/lib"
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists hiredis; then
        libs="$(pkg-config --libs hiredis)"
        cflags="$cflags $(pkg-config --cflags hiredis)"
    else
        libs="$fallback_lib -lhiredis"
        cflags="$cflags $fallback_inc"
    fi
    "$cxx" $cflags "$CTRL_SRC" -o "$CTRL_BIN" $libs
}

echo "[run_secondary_dt] Starting external controller (poll interval: ${DT2_POLL_INTERVAL_S}s)..."
ensure_sim_binary_fresh
build_cpp_controller
"$CTRL_BIN" "$SCRIPT_DIR" &
CTRL_PID=$!
echo "[run_secondary_dt] External controller C++ PID=$CTRL_PID"

# Give the controller one poll cycle to publish initial predictions before
# the simulation starts issuing Q-cycles.
sleep "${DT2_CTRL_WARMUP_S:-0.1}"

# Run the secondary digital-twin simulation.
# Extra args are forwarded to run_sim_portable.sh.
echo "[run_secondary_dt] Starting secondary simulation..."
./run_sim_portable.sh -u Cmdenv -c DT-Secondary-MotionChannel -r 0 --*.manager.port=10000 "$@" &
SIM_PID=$!
wait "$SIM_PID"
