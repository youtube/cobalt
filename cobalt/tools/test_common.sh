# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Common utilities for Cobalt integration tests on Linux.

MODE="modular"
BUILD=false
CONFIG="devel"

print_usage() {
  echo "Usage: $1 [options]"
  echo "Options:"
  echo "  --mode [monolithic|modular|evergreen]  Select Cobalt mode (default: modular)"
  echo "  --build                                Build before running test"
  echo "  --config [devel|debug|qa|gold]         Select build configuration (default: devel)"
  echo "  --help                                 Show this help message"
}

parse_args() {
  local script_name=$1
  shift
  while [[ $# -gt 0 ]]; do
    case $1 in
      --mode)
        MODE="$2"
        shift 2
        ;;
      --build)
        BUILD=true
        shift
        ;;
      --config)
        CONFIG="$2"
        shift 2
        ;;
      --help)
        print_usage "$script_name"
        exit 0
        ;;
      *)
        echo "Unknown argument: $1"
        print_usage "$script_name"
        exit 1
        ;;
    esac
  done

  case $MODE in
    monolithic)
      PLATFORM="linux-x64x11"
      EXECUTABLE_NAME="cobalt"
      ;;
    modular)
      PLATFORM="linux-x64x11-modular"
      EXECUTABLE_NAME="cobalt_loader"
      ;;
    evergreen)
      PLATFORM="evergreen-x64"
      EXECUTABLE_NAME="elf_loader_sandbox"
      ;;
    *)
      echo "Invalid mode: $MODE"
      exit 1
      ;;
  esac

  OUT_DIR="out/${PLATFORM}_${CONFIG}"

  if [ "$MODE" = "evergreen" ]; then
    EXECUTABLE="./cobalt/tools/run_evergreen_wrapper.sh"
    export TEST_OUT_DIR="./${OUT_DIR}"
  else
    EXECUTABLE="./${OUT_DIR}/${EXECUTABLE_NAME}"
  fi
}

check_running_processes() {
  local script_basename=$1
  local my_pgid=$(ps -o pgid= -p $$ | tr -d ' ')

  local first_char="${script_basename:0:1}"
  local rest="${script_basename:1}"
  local pattern="[$first_char]$rest"

  # Find other instances of this script
  local other_test_pids=$(pgrep -f "$pattern" | while read pid; do
    local pgid=$(ps -o pgid= -p $pid | tr -d ' ')
    if [ "$pgid" != "$my_pgid" ]; then echo $pid; fi
  done | xargs || true)

  local waiter_pids=$(pgrep -f "[w]ait_for_state.sh" | while read pid; do
    local pgid=$(ps -o pgid= -p $pid | tr -d ' ')
    if [ "$pgid" != "$my_pgid" ]; then echo $pid; fi
  done | xargs || true)

  local cobalt_pids=$(pgrep -x "$EXECUTABLE_NAME" | while read pid; do
    local pgid=$(ps -o pgid= -p $pid | tr -d ' ')
    if [ "$pgid" != "$my_pgid" ]; then echo $pid; fi
  done | xargs || true)

  if [ -n "$other_test_pids" ] || [ -n "$waiter_pids" ] || [ -n "$cobalt_pids" ]; then
    echo "FAILURE: Previous test instance or Cobalt process is still running."
    [ -n "$other_test_pids" ] && echo "Found $script_basename PIDs: $other_test_pids"
    [ -n "$waiter_pids" ] && echo "Found wait_for_state.sh PIDs: $waiter_pids"
    [ -n "$cobalt_pids" ] && echo "Found $EXECUTABLE_NAME PIDs: $cobalt_pids"
    echo "Please kill them before running this test."
    exit 1
  fi
}

run_build_if_needed() {
  if [ "$BUILD" = true ]; then
    echo "Building $EXECUTABLE_NAME for $PLATFORM..."

    local build_target=$EXECUTABLE_NAME
    if [ "$MODE" = "evergreen" ]; then
      build_target="cobalt"
    fi

    echo "Running: autoninja -C $OUT_DIR $build_target"
    autoninja -C "$OUT_DIR" "$build_target"
  fi
}
