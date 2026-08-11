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
# RDK Evergreen Memory Benchmark Script (with Mali GPU Debugfs Integration from PR 11014):
# Measures CPU and GPU memory across Foreground, Suspended (Background), and Resumed states.

readonly CALLSIGN="YouTube"
readonly PROCESS_NAME="YouTube"
readonly INTERVAL=1
readonly OUTPUT_FILE="rdk_memory_benchmark_$(date +%Y%m%d_%H%M%S).txt"
readonly JSONRPC_DELAY=2

echo "RDK Memory Benchmark Started at $(date)" > "$OUTPUT_FILE"
exec > >(tee -a "$OUTPUT_FILE") 2>&1

# Define scenarios: name|url|duration|rounds|action
SCENARIOS=(
    "Foreground Main Page|https://www.youtube.com/tv|30|1"
    "Background Suspended|https://www.youtube.com/tv|30|1|suspend"
    "Foreground Resumed|https://www.youtube.com/tv|30|1|resume"
)

calculate_median() {
    local arr=($(printf '%s\n' "$@" | sort -n))
    local count=${#arr[@]}
    if [ "$count" -eq 0 ]; then echo "0"; return; fi
    if (( count % 2 == 1 )); then
        echo "${arr[$((count/2))]}"
    else
        local mid=$((count/2))
        local val1="${arr[$((mid-1))]}"
        local val2="${arr[$mid]}"
        echo "$val1 $val2" | awk '{print ($1 + $2) / 2}'
    fi
}

calculate_peak() {
    local arr=($(printf '%s\n' "$@" | sort -n))
    local count=${#arr[@]}
    if [ "$count" -eq 0 ]; then echo "0"; return; fi
    echo "${arr[$((count-1))]}"
}

kb_to_mb() {
    local kb=$1
    if [ -z "$kb" ]; then
        echo "0.00"
    else
        echo "$kb" | awk '{printf "%.2f", $1 / 1024}'
    fi
}

calculate_average() {
    local arr=("$@")
    local count=${#arr[@]}
    if [ "$count" -eq 0 ]; then echo "0"; return; fi
    printf '%s\n' "${arr[@]}" | awk '{sum+=$1} END {print sum/NR}'
}

send_jsonrpc() {
    local method="$1"
    local params="$2"
    if [ -n "$params" ]; then
        curl -s -m 2 http://127.0.0.1:9998/jsonrpc -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"$method\",\"params\":$params}"
    else
        curl -s -m 2 http://127.0.0.1:9998/jsonrpc -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"$method\"}"
    fi
}

deactivate_plugin() {
    send_jsonrpc "Controller.1.deactivate" "{\"callsign\":\"$CALLSIGN\"}"
}

activate_plugin() {
    send_jsonrpc "Controller.1.activate" "{\"callsign\":\"$CALLSIGN\"}"
}

suspend_plugin() {
    send_jsonrpc "YouTube.1.state" "\"suspended\""
}

resume_plugin() {
    send_jsonrpc "YouTube.1.state" "\"resumed\""
}

configure_plugin_url() {
    local url="$1"
    local default_config="{\"url\":\"$url\",\"clientidentifier\":\"wst-Cobalt-0\",\"closurepolicy\":\"quit\",\"contentdir\":\"/data/out_cobalt\",\"systemproperties\":{\"modelname\":\"AH212\",\"brandname\":\"RDKCommonPort\",\"modelyear\":2025},\"root\":{\"mode\":\"Local\",\"locator\":\"libWPEFrameworkCobaltImpl.so\"}}"
    send_jsonrpc "Controller.1.configuration@$CALLSIGN" "$default_config"
}

launch_youtube_plugin() {
    local url="$1"
    echo "Configuring URL: $url"
    configure_plugin_url "$url"
    sleep $JSONRPC_DELAY
    echo "Activating YouTube plugin (callsign: $CALLSIGN)..."
    activate_plugin
    sleep 5
    PID=$(pgrep -n "$PROCESS_NAME")
}

scenario_names_ordered=()
scenario_data_ordered=()

for scenario_info in "${SCENARIOS[@]}"; do
    IFS='|' read -r scenario_name scenario_command scenario_duration scenario_rounds scenario_action <<< "$scenario_info"
    if [ -z "$scenario_rounds" ]; then scenario_rounds=1; fi

    echo ""
    echo "##########################################################"
    echo "SCENARIO: $scenario_name"
    echo "ACTION:   ${scenario_action:-none}"
    echo "##########################################################"

    if [ "$scenario_action" = "suspend" ]; then
        echo "Suspending YouTube plugin..."
        suspend_plugin
        sleep 3
    elif [ "$scenario_action" = "resume" ]; then
        echo "Resuming YouTube plugin..."
        resume_plugin
        sleep 3
    else
        echo "Launching fresh YouTube plugin..."
        deactivate_plugin
        sleep 2
        launch_youtube_plugin "$scenario_command"
    fi

    PID=$(pgrep -n "$PROCESS_NAME" || true)
    if [ -z "$PID" ]; then
        echo "Error: Process $PROCESS_NAME not running!"
        continue
    fi

    echo "Target PID: $PID"
    echo "Collecting samples for $scenario_duration seconds..."

    smaps_rss_samples=()
    status_rss_samples=()
    gpu_rss_samples=()
    gpu_profile_samples=()
    gpu_allocs_samples=()

    for ((i=1; i<=$scenario_duration; i++)); do
        if [ ! -d "/proc/$PID" ]; then
            echo "Process $PID exited unexpectedly."
            break
        fi

        # smaps / smaps_rollup
        if [ -f "/proc/$PID/smaps_rollup" ]; then
            s_rss=$(awk '/^Rss:/ {rss+=$2} END {printf "%.0f", rss}' "/proc/$PID/smaps_rollup" 2>/dev/null)
        else
            s_rss=$(awk '/^Rss:/ {rss+=$2} END {printf "%.0f", rss}' "/proc/$PID/smaps" 2>/dev/null)
        fi

        # /proc/$PID/status
        t_rss=$(awk '/^VmRSS:/ {print $2}' "/proc/$PID/status" 2>/dev/null)

        # GPU Memory: Page tables
        g_rss=0
        if [ -f "/sys/kernel/debug/mali0/gpu_memory" ]; then
            gpu_pages=$(awk -v pid="$PID" '$2 == pid {sum+=$3} END {print sum}' /sys/kernel/debug/mali0/gpu_memory 2>/dev/null)
            if [[ "$gpu_pages" =~ ^[0-9]+$ ]]; then
                g_rss=$((gpu_pages * 4))
            fi
        fi

        # GPU Memory: Context debugfs (PR 11014 additions)
        ctx_dir=""
        for d in /sys/kernel/debug/mali0/ctx/${PID}_*; do
            if [ -d "$d" ]; then
                ctx_dir="$d"
                break
            fi
        done

        gp_rss=0
        if [ -n "$ctx_dir" ] && [ -f "$ctx_dir/mem_profile" ]; then
            gp_val=$(awk '/Total allocated GPU memory:/ {print $NF}' "$ctx_dir/mem_profile" 2>/dev/null)
            if [[ "$gp_val" =~ ^[0-9]+$ ]]; then
                gp_rss=$((gp_val / 1024))
            fi
        fi

        ga_rss=0
        if [ -n "$ctx_dir" ] && [ -f "$ctx_dir/mem_allocs" ]; then
            ga_val=$(awk '{sum += $2} END {print sum}' "$ctx_dir/mem_allocs" 2>/dev/null)
            if [[ "$ga_val" =~ ^[0-9]+$ ]]; then
                ga_rss=$((ga_val / 1024))
            fi
        fi

        smaps_rss_samples+=("${s_rss:-0}")
        status_rss_samples+=("${t_rss:-0}")
        gpu_rss_samples+=("${g_rss:-0}")
        gpu_profile_samples+=("${gp_rss:-0}")
        gpu_allocs_samples+=("${ga_rss:-0}")

        printf "[%02d/%02d] smaps RSS: %s MB | status RSS: %s MB | GPU (page tab): %s MB | GPU (allocs): %s MB | GPU (profile): %s MB\n" \
            "$i" "$scenario_duration" \
            "$(kb_to_mb $s_rss)" "$(kb_to_mb $t_rss)" "$(kb_to_mb $g_rss)" \
            "$(kb_to_mb $ga_rss)" "$(kb_to_mb $gp_rss)"

        sleep 1
    done

    s_med=$(calculate_median "${smaps_rss_samples[@]}")
    t_med=$(calculate_median "${status_rss_samples[@]}")
    g_med=$(calculate_median "${gpu_rss_samples[@]}")
    ga_med=$(calculate_median "${gpu_allocs_samples[@]}")
    gp_med=$(calculate_median "${gpu_profile_samples[@]}")

    scenario_names_ordered+=("$scenario_name")
    scenario_data_ordered+=("smaps:$s_med;status:$t_med;gpu_pt:$g_med;gpu_allocs:$ga_med;gpu_profile:$gp_med")
done

echo ""
echo "=========================================================="
echo "                 RDK MEMORY BENCHMARK REPORT"
echo "=========================================================="
for ((i=0; i<${#scenario_names_ordered[@]}; i++)); do
    name="${scenario_names_ordered[$i]}"
    data="${scenario_data_ordered[$i]}"
    s_med=$(echo "$data" | awk -F'[;:]' '{print $2}')
    t_med=$(echo "$data" | awk -F'[;:]' '{print $4}')
    g_med=$(echo "$data" | awk -F'[;:]' '{print $6}')
    ga_med=$(echo "$data" | awk -F'[;:]' '{print $8}')
    gp_med=$(echo "$data" | awk -F'[;:]' '{print $10}')

    printf "Scenario: %-25s | smaps: %7s MB | status: %7s MB | GPU pt: %7s MB | GPU alloc: %7s MB\n" \
        "$name" "$(kb_to_mb $s_med)" "$(kb_to_mb $t_med)" "$(kb_to_mb $g_med)" "$(kb_to_mb $ga_med)"
done
echo "=========================================================="
