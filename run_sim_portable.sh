#!/bin/bash
# Portable runner for the IoV Digital Twin Task Offloading simulation.
# Uses env vars or auto-detection to avoid hard-coded paths.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pick_first_dir() {
  for cand in "$@"; do
    if [ -d "$cand" ]; then
      echo "$cand"
      return 0
    fi
  done
  return 1
}

find_inet_path() {
  if [ -n "${INET_PATH:-}" ] && [ -d "$INET_PATH" ]; then
    echo "$INET_PATH"
    return 0
  fi

  local base
  for base in "$SCRIPT_DIR/.." "$SCRIPT_DIR/../.." "$HOME"; do
    for cand in "$base"/inet*; do
      if [ -d "$cand/src" ] && [ -d "$cand/src/inet" -o -f "$cand/src/INET" ]; then
        echo "$cand"
        return 0
      fi
    done
  done
  return 1
}

find_veins_path() {
  if [ -n "${VEINS_PATH:-}" ] && [ -d "$VEINS_PATH" ]; then
    echo "$VEINS_PATH"
    return 0
  fi

  local base
  for base in "$SCRIPT_DIR/.." "$SCRIPT_DIR/../.." "$HOME"; do
    for cand in "$base"/veins*; do
      if [ -d "$cand/src/veins" ]; then
        echo "$cand"
        return 0
      fi
    done
  done
  return 1
}

find_omnetpp_home() {
  if [ -n "${OMNETPP_HOME:-}" ] && [ -f "$OMNETPP_HOME/setenv" ]; then
    echo "$OMNETPP_HOME"
    return 0
  fi

  pick_first_dir \
    "$HOME/omnetpp-6.1" \
    "$HOME/omnetpp" \
    "/opt/omnetpp-6.1" \
    "/opt/omnetpp" || return 1
}

OMNETPP_HOME="$(find_omnetpp_home || true)"
if [ -z "$OMNETPP_HOME" ] || [ ! -f "$OMNETPP_HOME/setenv" ]; then
  echo "Error: OMNeT++ not found. Set OMNETPP_HOME to your OMNeT++ install directory." >&2
  exit 1
fi

INET_PATH="$(find_inet_path || true)"
if [ -z "$INET_PATH" ] || [ ! -d "$INET_PATH/src" ]; then
  echo "Error: INET not found. Set INET_PATH to your INET root directory." >&2
  exit 1
fi

VEINS_PATH="$(find_veins_path || true)"
if [ -z "$VEINS_PATH" ] || [ ! -d "$VEINS_PATH/src/veins" ]; then
  echo "Error: Veins not found. Set VEINS_PATH to your Veins root directory." >&2
  exit 1
fi

set +u
source "$OMNETPP_HOME/setenv"
set -u

cd "$SCRIPT_DIR"

# === PREFLIGHT CHECK: Clean up stale SUMO processes on port 9999 ===
# TraCI uses port 9999. If sumo-launchd.py is running from a previous session,
# it will cause "TraCI API version 1" errors. Kill it before starting.
PORT_CHECK=$(ss -ltnp 2>/dev/null | grep ':9999' || true)
if [ -n "$PORT_CHECK" ]; then
  echo "⚠ Detected process on port 9999. Checking if it's sumo-launchd..."
  PORT_PID=$(echo "$PORT_CHECK" | grep -oP 'pid=\K[0-9]+' | head -1)
  if [ -n "$PORT_PID" ]; then
    PROCESS_CMD=$(ps -fp "$PORT_PID" 2>/dev/null | grep -v "PID" | awk '{print $8, $9, $10}' || true)
    if echo "$PROCESS_CMD" | grep -q "sumo-launchd"; then
      echo "  └─ Found stale sumo-launchd.py (PID $PORT_PID). Terminating..."
      kill "$PORT_PID" 2>/dev/null || true
      sleep 1
      echo "  └─ Port 9999 cleared for TraCI."
    else
      echo "  └─ Warning: Port 9999 occupied by: $PROCESS_CMD"
      echo "  └─ This may cause TraCI connection errors. Kill manually if needed:"
      echo "     kill $PORT_PID"
    fi
  fi
fi

echo "Starting IoV Digital Twin Task Offloading simulation..."
echo "OMNeT++: $OMNETPP_HOME"
echo "INET:    $INET_PATH"
echo "Veins:   $VEINS_PATH"
echo "NED path: .:$INET_PATH/src:$VEINS_PATH/src/veins"
echo ""

./IoV-Digital-Twin-TaskOffloading \
  -n ".:$INET_PATH/src:$VEINS_PATH/src/veins" \
  -l "$INET_PATH/src/INET" \
  -l "$VEINS_PATH/src/veins" \
  omnetpp.ini "$@"
