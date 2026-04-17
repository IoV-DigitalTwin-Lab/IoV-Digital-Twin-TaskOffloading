#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Keep launcher display and controller runtime default aligned (100 ms).
export DT2_POLL_INTERVAL_S="${DT2_POLL_INTERVAL_S:-0.1}"

CTRL_PID=""
cleanup() {
    if [[ -n "$CTRL_PID" ]] && kill -0 "$CTRL_PID" 2>/dev/null; then
        echo "[run_secondary_dt] Stopping external controller (PID $CTRL_PID)..."
        kill "$CTRL_PID" 2>/dev/null || true
        wait "$CTRL_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

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
./run_sim_portable.sh -u Cmdenv -c DT-Secondary-MotionChannel -r 0 --*.manager.port=10000 "$@"
