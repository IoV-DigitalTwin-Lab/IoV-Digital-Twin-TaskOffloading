#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Runs the secondary digital-twin simulation (mobility + channel only).
# Extra args are forwarded to run_sim_portable.sh.
exec ./run_sim_portable.sh -u Cmdenv -c DT-Secondary-MotionChannel "$@"
