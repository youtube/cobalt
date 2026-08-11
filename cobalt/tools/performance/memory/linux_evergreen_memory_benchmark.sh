#!/bin/bash
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Linux Evergreen Memory Benchmark Script:
# Measures CPU and GPU memory across Foreground, Concealed (Background), and Restored states.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

BUILD_DIR_ARG="${1:-${SRC_DIR}/out/evergreen-x64_qa}"
if [[ "$BUILD_DIR_ARG" != /* ]]; then
    BUILD_DIR="${SRC_DIR}/${BUILD_DIR_ARG}"
else
    BUILD_DIR="${BUILD_DIR_ARG}"
fi

URL="${2:-https://www.youtube.com/tv}"
WARMUP_SECS="${3:-20}"
MEASURE_SECS="${4:-15}"

if [ ! -f "${BUILD_DIR}/loader_app" ]; then
    echo "Error: ${BUILD_DIR}/loader_app not found!"
    exit 1
fi

export DISPLAY="${DISPLAY:-:100}"

echo "=========================================================="
echo " Starting Linux Evergreen Memory Benchmark"
echo " Build:   ${BUILD_DIR}"
echo " URL:     ${URL}"
echo " Display: ${DISPLAY}"
echo " Warmup:  ${WARMUP_SECS}s | Measure: ${MEASURE_SECS}s"
echo " Env:     LIBGL_ALWAYS_SOFTWARE=${LIBGL_ALWAYS_SOFTWARE:-0}"
echo " Date:    $(date)"
echo "=========================================================="

# Start Cobalt in background
LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-0}" "${BUILD_DIR}/loader_app" --url="${URL}" > /tmp/evergreen_qa_bench.log 2>&1 &
APP_PID=$!

cleanup() {
    if kill -0 "$APP_PID" 2>/dev/null; then
        echo "Terminating test app (PID: $APP_PID)..."
        kill -9 "$APP_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "Waiting for app to initialize (PID: $APP_PID)..."
sleep 3

if ! kill -0 "$APP_PID" 2>/dev/null; then
    echo "Error: loader_app died immediately!"
    cat /tmp/evergreen_qa_bench.log
    exit 1
fi

sample_memory() {
    local target_pid=$1
    local label=$2
    local duration=$3
    local rss_samples=()
    local pss_samples=()
    local pdirty_samples=()
    local vmdata_samples=()
    local gpu_map_samples=()

    echo "--- Sampling ${label} for ${duration} seconds ---"
    for ((s=1; s<=duration; s++)); do
        if ! kill -0 "$target_pid" 2>/dev/null; then
            echo "Process $target_pid exited during sampling!"
            break
        fi

        # Extract memory metrics from smaps_rollup if available, else smaps
        local rss=0
        local pss=0
        local pdirty=0
        local vmdata=0
        local gpu_maps=0

        if [ -f "/proc/$target_pid/smaps_rollup" ]; then
            rss=$(awk '/^Rss:/ {print $2}' "/proc/$target_pid/smaps_rollup" 2>/dev/null || echo 0)
            pss=$(awk '/^Pss:/ {print $2}' "/proc/$target_pid/smaps_rollup" 2>/dev/null || echo 0)
            pdirty=$(awk '/^Private_Dirty:/ {print $2}' "/proc/$target_pid/smaps_rollup" 2>/dev/null || echo 0)
        elif [ -f "/proc/$target_pid/smaps" ]; then
            rss=$(awk '/^Rss:/ {sum+=$2} END {print sum}' "/proc/$target_pid/smaps" 2>/dev/null || echo 0)
            pss=$(awk '/^Pss:/ {sum+=$2} END {print sum}' "/proc/$target_pid/smaps" 2>/dev/null || echo 0)
            pdirty=$(awk '/^Private_Dirty:/ {sum+=$2} END {print sum}' "/proc/$target_pid/smaps" 2>/dev/null || echo 0)
        fi

        if [ -f "/proc/$target_pid/status" ]; then
            vmdata=$(awk '/^VmData:/ {print $2}' "/proc/$target_pid/status" 2>/dev/null || echo 0)
        fi

        # Extract mapped GPU driver buffers (dri/drm/nvidia/mesa/render)
        if [ -f "/proc/$target_pid/smaps" ]; then
            gpu_maps=$(awk '/(dri|drm|nvidia|render|mali)/ {getline; if ($1 == "Size:") sum+=$2} END {print sum+0}' "/proc/$target_pid/smaps" 2>/dev/null || echo 0)
        fi

        rss_samples+=("${rss:-0}")
        pss_samples+=("${pss:-0}")
        pdirty_samples+=("${pdirty:-0}")
        vmdata_samples+=("${vmdata:-0}")
        gpu_map_samples+=("${gpu_maps:-0}")

        printf "  [%02d/%02d] RSS: %6.2f MB | PSS: %6.2f MB | PrivDirty: %6.2f MB | VmData: %6.2f MB | GPU Mappings: %6.2f MB\n" \
            "$s" "$duration" \
            "$(echo "${rss:-0}" | awk '{printf "%.2f", $1/1024}')" \
            "$(echo "${pss:-0}" | awk '{printf "%.2f", $1/1024}')" \
            "$(echo "${pdirty:-0}" | awk '{printf "%.2f", $1/1024}')" \
            "$(echo "${vmdata:-0}" | awk '{printf "%.2f", $1/1024}')" \
            "$(echo "${gpu_maps:-0}" | awk '{printf "%.2f", $1/1024}')"

        sleep 1
    done

    # Calculate median
    calc_median() {
        local arr=($(printf '%s\n' "$@" | sort -n))
        local cnt=${#arr[@]}
        if [ "$cnt" -eq 0 ]; then echo "0"; return; fi
        local mid=$((cnt/2))
        echo "${arr[$mid]}"
    }

    local med_rss=$(calc_median "${rss_samples[@]}")
    local med_pss=$(calc_median "${pss_samples[@]}")
    local med_pdirty=$(calc_median "${pdirty_samples[@]}")
    local med_vmdata=$(calc_median "${vmdata_samples[@]}")
    local med_gpu=$(calc_median "${gpu_map_samples[@]}")

    eval "${label}_MED_RSS=$med_rss"
    eval "${label}_MED_PSS=$med_pss"
    eval "${label}_MED_PDIRTY=$med_pdirty"
    eval "${label}_MED_VMDATA=$med_vmdata"
    eval "${label}_MED_GPU=$med_gpu"
}

echo "Warming up application (${WARMUP_SECS}s)..."
sleep "$WARMUP_SECS"

# 1. Measure Foreground
sample_memory "$APP_PID" "FOREGROUND" "$MEASURE_SECS"

# 2. Trigger Conceal (Background) via SIGUSR2
echo ""
echo ">>> Sending SIGUSR2 (Conceal/Background) to PID $APP_PID..."
kill -SIGUSR2 "$APP_PID"
sleep 3
sample_memory "$APP_PID" "BACKGROUND" "$MEASURE_SECS"

# 3. Trigger Reveal (Restore) via SIGCONT
echo ""
echo ">>> Sending SIGCONT (Reveal/Restore) to PID $APP_PID..."
kill -SIGCONT "$APP_PID"
sleep 3
sample_memory "$APP_PID" "RESTORED" "$MEASURE_SECS"

kb_to_mb() {
    echo "$1" | awk '{printf "%.2f", $1/1024}'
}

echo ""
echo "=========================================================="
echo "               FINAL MEMORY BENCHMARK REPORT"
echo " Build: ${BUILD_DIR}"
echo " URL:   ${URL}"
echo "=========================================================="
printf "%-22s | %-12s | %-12s | %-12s | %-12s\n" "Lifecycle State" "Median RSS" "Median PSS" "Private Dirty" "VmData (Heap)"
echo "---------------------------------------------------------------------------------------"
printf "%-22s | %10s MB | %10s MB | %10s MB | %10s MB\n" "Foreground (Active)" "$(kb_to_mb $FOREGROUND_MED_RSS)" "$(kb_to_mb $FOREGROUND_MED_PSS)" "$(kb_to_mb $FOREGROUND_MED_PDIRTY)" "$(kb_to_mb $FOREGROUND_MED_VMDATA)"
printf "%-22s | %10s MB | %10s MB | %10s MB | %10s MB\n" "Background (Concealed)" "$(kb_to_mb $BACKGROUND_MED_RSS)" "$(kb_to_mb $BACKGROUND_MED_PSS)" "$(kb_to_mb $BACKGROUND_MED_PDIRTY)" "$(kb_to_mb $BACKGROUND_MED_VMDATA)"
printf "%-22s | %10s MB | %10s MB | %10s MB | %10s MB\n" "Restored (Revealed)" "$(kb_to_mb $RESTORED_MED_RSS)" "$(kb_to_mb $RESTORED_MED_PSS)" "$(kb_to_mb $RESTORED_MED_PDIRTY)" "$(kb_to_mb $RESTORED_MED_VMDATA)"
echo "---------------------------------------------------------------------------------------"

DIFF_RSS=$((FOREGROUND_MED_RSS - BACKGROUND_MED_RSS))
DIFF_PSS=$((FOREGROUND_MED_PSS - BACKGROUND_MED_PSS))
DIFF_PDIRTY=$((FOREGROUND_MED_PDIRTY - BACKGROUND_MED_PDIRTY))

printf "SAVINGS ON CONCEAL: RSS: -%s MB | PSS: -%s MB | PrivDirty: -%s MB\n" \
    "$(kb_to_mb $DIFF_RSS)" "$(kb_to_mb $DIFF_PSS)" "$(kb_to_mb $DIFF_PDIRTY)"
echo "=========================================================="
