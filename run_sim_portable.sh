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
    "/opt/omnet/omnetpp-6.1" \
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

echo "Starting IoV Digital Twin Task Offloading simulation..."
echo "OMNeT++: $OMNETPP_HOME"
echo "INET:    $INET_PATH"
echo "Veins:   $VEINS_PATH"
echo "NED path: .:$INET_PATH/src:$VEINS_PATH/src/veins"
echo ""

# Detect if running in headless environment
UI_ENV="Qtenv"
if [ -z "${DISPLAY:-}" ]; then
  echo "Headless environment detected (no DISPLAY). Using Cmdenv (command-line mode)."
  UI_ENV="Cmdenv"
else
  echo "GUI mode (Qtenv) available."
fi

# Allow override via environment or command-line
if [ "${HEADLESS_MODE:-0}" = "1" ] || [ "${CMDENV_MODE:-0}" = "1" ]; then
  UI_ENV="Cmdenv"
fi

# The executable is already linked against INET and Veins.
# Loading them again with -l can register a second copy and crash on shutdown.
./IoV-Digital-Twin-TaskOffloading \
  -u "$UI_ENV" \
  -n ".:$INET_PATH/src:$VEINS_PATH/src/veins" \
  omnetpp.ini "$@"
