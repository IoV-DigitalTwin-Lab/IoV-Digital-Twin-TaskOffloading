#!/bin/bash
# Portable runner for the IoV Digital Twin Task Offloading simulation.
# Uses env vars or auto-detection to avoid hard-coded paths.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CTRL_PID=""
SUMO_CFG_BACKUP=""

build_cpp_controller() {
  local ctrl_bin="$SCRIPT_DIR/external_controller_cpp"
  local ctrl_src="$SCRIPT_DIR/external_controller.cpp"
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

  "$cxx" $cflags "$ctrl_src" -o "$ctrl_bin" $libs
}

extract_config_name() {
  local prev=""
  for arg in "$@"; do
    if [ "$prev" = "-c" ]; then
      echo "$arg"
      return 0
    fi
    prev="$arg"
  done
  echo ""
}

cleanup() {
  if [ -n "$CTRL_PID" ] && kill -0 "$CTRL_PID" 2>/dev/null; then
    echo "Stopping external controller (PID $CTRL_PID)..."
    kill "$CTRL_PID" 2>/dev/null || true
    wait "$CTRL_PID" 2>/dev/null || true
  fi

  if [ -n "$SUMO_CFG_BACKUP" ] && [ -f "$SUMO_CFG_BACKUP" ]; then
    cp "$SUMO_CFG_BACKUP" "$SCRIPT_DIR/erlangen.sumo.cfg"
  fi
}
trap cleanup EXIT INT TERM

configure_runtime_traffic() {
  local traffic_scale="${TRAFFIC_SCALE:-1}"
  local flow_period_override="${FLOW_PERIOD:-}"
  local initial_scale="${INITIAL_VEHICLE_SCALE:-$traffic_scale}"

  if [ "$traffic_scale" = "1" ] && [ -z "$flow_period_override" ] && [ "$initial_scale" = "1" ]; then
    return 0
  fi

  local runtime_dir="$SCRIPT_DIR/.runtime"
  local route_in="$SCRIPT_DIR/erlangen.rou.xml"
  local route_out="$runtime_dir/erlangen.runtime.rou.xml"
  local cfg_out="$runtime_dir/erlangen.runtime.sumo.cfg"
  local cfg_in="$SCRIPT_DIR/erlangen.sumo.cfg"
  local cfg_backup="$runtime_dir/erlangen.sumo.cfg.bak"

  mkdir -p "$runtime_dir"

  python3 - "$route_in" "$route_out" "$cfg_in" "$cfg_out" \
    "$traffic_scale" "$flow_period_override" "$initial_scale" <<'PY'
import copy
import math
import sys
import xml.etree.ElementTree as ET

route_in, route_out, cfg_in, cfg_out, traffic_scale_raw, flow_period_raw, initial_scale_raw = sys.argv[1:8]

traffic_scale = float(traffic_scale_raw)
initial_scale = float(initial_scale_raw)
flow_period_override = float(flow_period_raw) if flow_period_raw else None

if traffic_scale <= 0 or initial_scale <= 0:
    raise SystemExit("TRAFFIC_SCALE and INITIAL_VEHICLE_SCALE must be > 0")

route_tree = ET.parse(route_in)
route_root = route_tree.getroot()

vehicles = [elem for elem in route_root.findall("vehicle")]
flows = [elem for elem in route_root.findall("flow")]

keep_count = max(1, int(round(len(vehicles) * initial_scale))) if vehicles else 0
new_vehicle_elems = []

for idx in range(keep_count):
    src = vehicles[idx % len(vehicles)]
    elem = copy.deepcopy(src)
    if idx >= len(vehicles):
        base_depart = float(elem.get("depart", "0"))
        elem.set("id", f"{src.get('id', 'v')}_x{idx}")
        elem.set("depart", f"{base_depart + 0.01 * (idx - len(vehicles) + 1):.2f}")
    new_vehicle_elems.append(elem)

for elem in vehicles:
    route_root.remove(elem)

insert_at = 0
for idx, elem in enumerate(list(route_root)):
    if elem.tag == "vehicle":
        insert_at = idx
        break
    insert_at = idx + 1

for offset, elem in enumerate(new_vehicle_elems):
    route_root.insert(insert_at + offset, elem)

for flow in flows:
    if flow_period_override is not None:
        new_period = flow_period_override
    else:
        base_period = float(flow.get("period", "1"))
        new_period = max(1.0, base_period / traffic_scale)
    flow.set("period", f"{new_period:.6f}".rstrip("0").rstrip("."))

route_tree.write(route_out, encoding="utf-8", xml_declaration=True)

cfg_tree = ET.parse(cfg_in)
cfg_root = cfg_tree.getroot()
input_node = cfg_root.find("input")
if input_node is None:
    raise SystemExit("SUMO config missing <input> section")

for child in input_node:
    if child.tag == "route-files":
        child.set("value", route_out)

cfg_tree.write(cfg_out, encoding="utf-8", xml_declaration=True)
PY

  cp "$cfg_in" "$cfg_backup"
  cp "$cfg_out" "$cfg_in"
  SUMO_CFG_BACKUP="$cfg_backup"

  echo "Traffic scaling enabled:"
  echo "  TRAFFIC_SCALE=${traffic_scale}"
  echo "  INITIAL_VEHICLE_SCALE=${initial_scale}"
  if [ -n "$flow_period_override" ]; then
    echo "  FLOW_PERIOD=${flow_period_override}"
  fi
  echo "  Runtime route file: $route_out"
}

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

  if [ -d "/opt/omnet/veins/src/veins" ]; then
    echo "/opt/omnet/veins"
    return 0
  fi
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
INET_PATH="$(realpath "$INET_PATH")"

VEINS_PATH="$(find_veins_path || true)"
if [ -z "$VEINS_PATH" ] || [ ! -d "$VEINS_PATH/src/veins" ]; then
  echo "Error: Veins not found. Set VEINS_PATH to your Veins root directory." >&2
  exit 1
fi
VEINS_PATH="$(realpath "$VEINS_PATH")"

set +u
source "$OMNETPP_HOME/setenv"
set -u

# Ensure locally installed hiredis is available at runtime for external_controller_cpp.
if [ -d "/home/mihiraja/.local/lib" ]; then
  export LD_LIBRARY_PATH="/home/mihiraja/.local/lib:${LD_LIBRARY_PATH:-}"
fi

# OMNeT++ setenv may not include SUMO's binary directory on some systems.
if ! command -v sumo >/dev/null 2>&1; then
  if [ -x "/usr/share/sumo/bin/sumo" ]; then
    export PATH="/usr/share/sumo/bin:$PATH"
  fi
fi

if ! command -v sumo >/dev/null 2>&1; then
  echo "Error: SUMO executable not found in PATH. Install SUMO or add it to PATH." >&2
  exit 1
fi

export SUMO_HOME="${SUMO_HOME:-/usr/share/sumo}"

cd "$SCRIPT_DIR"

configure_runtime_traffic

RUN_CONFIG="$(extract_config_name "$@")"
AUTO_CONTROLLER="${DT2_AUTO_CONTROLLER:-1}"

# For primary run, keep external predictor alive so dt2:pred keys are populated.
if [ "$AUTO_CONTROLLER" = "1" ] && [ "$RUN_CONFIG" = "DT-PreSync" ]; then
  if pgrep -f "[e]xternal_controller_cpp" >/dev/null 2>&1; then
    echo "External controller already running; reusing existing process."
  else
    echo "Starting external controller for DT-PreSync future predictions..."
    build_cpp_controller
    "$SCRIPT_DIR/external_controller_cpp" "$SCRIPT_DIR" &
    CTRL_PID=$!
    echo "External controller PID: $CTRL_PID"
    sleep "${DT2_CTRL_WARMUP_S:-0.1}"
  fi
fi

NED_PATH=".:$INET_PATH/src:$VEINS_PATH/src/veins"
# Keep NED roots deterministic to avoid duplicate loads from mixed installs.
export NEDPATH="$NED_PATH"

echo "Starting IoV Digital Twin Task Offloading simulation..."
echo "OMNeT++: $OMNETPP_HOME"
echo "INET:    $INET_PATH"
echo "Veins:   $VEINS_PATH"
echo "SUMO:    $(command -v sumo)"
echo "NED path: $NED_PATH"
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
  -n "$NED_PATH" \
  omnetpp.ini "$@"
