#!/usr/bin/env bash
# Compatibility launcher: delegate to the portable runner that auto-detects
# OMNeT++/INET/Veins paths and avoids stale hard-coded NED roots.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

exec ./run_sim_portable.sh "$@"
