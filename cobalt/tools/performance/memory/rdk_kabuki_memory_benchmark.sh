#!/bin/bash

# TODO(b/522305776): Rewrite this script in Python to share common helper methods
# with deploy_rdk.py (e.g., launching/deactivating plugins, restarting wpeframework).

# Configuration
readonly CALLSIGN="YouTube"
readonly PROCESS_NAME="YouTube"
readonly INTERVAL=1
readonly OUTPUT_FILE="memory_test_results_kabuki_$(date +%Y%m%d).txt"
# Delay to allow WPEFramework to process state changes and configuration updates
readonly JSONRPC_DELAY=2

# Initialize output file
echo "Memory Test Started at $(date)" > "$OUTPUT_FILE"

# Redirect stdout and stderr to both terminal and output file
exec > >(tee -a "$OUTPUT_FILE") 2>&1

# Define scenarios: name|url|duration|rounds|action
# If rounds is omitted, it defaults to 1.
# Supported actions:
#   - scroll: Presses the 'right' key (sendkey right) every second.
#   - playback_scroll: Plays video for 1 minute, clicks down twice, then presses 'right' every second for 1 minutes.
SCENARIOS=(
    "Default Page|https://www.youtube.com/tv|60|5"
    "About Blank|about:blank|30|1"
    "4K Video Playback|https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ|180|5"
    "1080p Video Playback|https://www.youtube.com/tv#/watch?v=ou0cmY8Fqd0|180|5"
    "Home Page Scroll|https://www.youtube.com/tv|120|5|scroll"
    "4K Playback Scroll|https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ|120|5|playback_scroll"
    "1080p Playback Scroll|https://www.youtube.com/tv#/watch?v=ou0cmY8Fqd0|120|5|playback_scroll"
)

# Function to calculate median
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
        # Use awk for calculation to avoid bc dependency
        echo "$val1 $val2" | awk '{print ($1 + $2) / 2}'
    fi
}

# Function to calculate peak
calculate_peak() {
    local arr=($(printf '%s\n' "$@" | sort -n))
    local count=${#arr[@]}
    if [ "$count" -eq 0 ]; then echo "0"; return; fi
    echo "${arr[$((count-1))]}"
}

# Function to convert KB to MB
kb_to_mb() {
    local kb=$1
    if [ -z "$kb" ]; then
        echo "0.00"
    else
        echo "$kb" | awk '{printf "%.2f", $1 / 1024}'
    fi
}

# Function to calculate average of an array
calculate_average() {
    local arr=("$@")
    local count=${#arr[@]}
    if [ "$count" -eq 0 ]; then echo "0"; return; fi
    printf '%s\n' "${arr[@]}" | awk '{sum+=$1} END {print sum/NR}'
}

# JSON-RPC Helpers
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

configure_plugin_url() {
    local url="$1"
    if [ -z "$ORIGINAL_CONFIG" ]; then
        local default_config="{\"url\":\"$url\",\"clientidentifier\":\"wst-Cobalt-0\",\"closurepolicy\":\"quit\",\"contentdir\":\"/data/out_cobalt\",\"systemproperties\":{\"modelname\":\"AH212\",\"brandname\":\"RDKCommonPort\",\"modelyear\":2025},\"root\":{\"mode\":\"Local\",\"locator\":\"libWPEFrameworkCobaltImpl.so\"}}"
        send_jsonrpc "Controller.1.configuration@$CALLSIGN" "$default_config"
    else
        local updated_config
        updated_config=$(python3 -c "
import sys, json
try:
    data = json.loads(sys.argv[1])
    config = data.get('result', data)
    config['url'] = sys.argv[2]
    if 'root' in config and config['root'].get('mode') == 'Container':
        config['root']['mode'] = 'Local'
    print(json.dumps(config))
except Exception as e:
    sys.exit(1)
" "$ORIGINAL_CONFIG" "$url")

        if [ $? -eq 0 ] && [ -n "$updated_config" ]; then
            send_jsonrpc "Controller.1.configuration@$CALLSIGN" "$updated_config"
        else
            echo "Error updating config with python. Using fallback config."
            local default_config="{\"url\":\"$url\",\"clientidentifier\":\"wst-Cobalt-0\",\"closurepolicy\":\"quit\",\"contentdir\":\"/data/out_cobalt\",\"systemproperties\":{\"modelname\":\"AH212\",\"brandname\":\"RDKCommonPort\",\"modelyear\":2025},\"root\":{\"mode\":\"Local\",\"locator\":\"libWPEFrameworkCobaltImpl.so\"}}"
            send_jsonrpc "Controller.1.configuration@$CALLSIGN" "$default_config"
        fi
    fi
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

recover_wpeframework_and_launch() {
    local url="$1"
    echo "Warning: Process '$PROCESS_NAME' not found after launch. Attempting to recover by restarting wpeframework..."
    systemctl restart wpeframework
    echo "Waiting for WPEFramework JSON-RPC to become responsive..."
    local max_wait=30
    while [ $max_wait -gt 0 ]; do
        if send_jsonrpc "Controller.1.configuration@$CALLSIGN" "" | grep -q "result"; then
            echo "WPEFramework is ready!"
            break
        fi
        sleep 2
        ((max_wait--))
    done
    # Re-fetch the fresh configuration after restart
    ORIGINAL_CONFIG=$(send_jsonrpc "Controller.1.configuration@$CALLSIGN" "")
    if ! echo "$ORIGINAL_CONFIG" | grep -q '"result"'; then
        ORIGINAL_CONFIG=""
    fi

    # Retry launch
    launch_youtube_plugin "$url"

    if [ -z "$PID" ]; then
        return 1
    fi
    return 0
}

# Fetch original configuration at start
ORIGINAL_CONFIG=$(send_jsonrpc "Controller.1.configuration@$CALLSIGN" "")
if ! echo "$ORIGINAL_CONFIG" | grep -q '"result"'; then
    echo "Warning: Could not fetch original configuration for $CALLSIGN. Using defaults."
    ORIGINAL_CONFIG=""
fi

# Store final results for each scenario using parallel arrays for compatibility
scenario_names_ordered=()
scenario_data_ordered=()
scenario_rounds_ordered=()

for scenario_info in "${SCENARIOS[@]}"; do
    IFS='|' read -r scenario_name scenario_command scenario_duration scenario_rounds scenario_action <<< "$scenario_info"

    # If rounds is omitted, it defaults to 1.
    if [ -z "$scenario_rounds" ]; then
        scenario_rounds=1
    fi

    echo ""
    echo "##########################################################"
    echo "SCENARIO: $scenario_name"
    echo "URL:      $scenario_command"
    echo "DURATION: $scenario_duration seconds"
    echo "ROUNDS:   $scenario_rounds"
    echo "##########################################################"

    round_smaps_median_rss=()
    round_smaps_peak_rss=()
    round_v8_median_rss=()
    round_v8_peak_rss=()
    round_status_median_rss=()
    round_status_peak_rss=()
    round_gpu_median_rss=()
    round_gpu_peak_rss=()

    for ((round=1; round<=$scenario_rounds; round++)); do
        echo "=========================================================="
        echo "Starting Round $round/$scenario_rounds"

        echo "Deactivating YouTube plugin (callsign: $CALLSIGN)..."
        deactivate_plugin
        sleep $JSONRPC_DELAY

        # Attempt first launch
        launch_youtube_plugin "$scenario_command"

        # If the process is not found, restart wpeframework and attempt recovery
        if [ -z "$PID" ]; then
            if ! recover_wpeframework_and_launch "$scenario_command"; then
                echo "Error: Process '$PROCESS_NAME' still not found after restart recovery. Skipping round."
                continue
            fi
        fi

        echo "Target PID: $PID"
        echo "Collecting samples for $scenario_duration seconds..."

        smaps_rss_samples=()
        v8_rss_samples=()
        status_rss_samples=()
        gpu_rss_samples=()

        for ((i=1; i<=$scenario_duration; i++)); do
            if [ ! -d "/proc/$PID" ]; then
                echo "Process $PID exited unexpectedly."
                break
            fi

            # --- smaps / smaps_rollup ---
            if [ -f "/proc/$PID/smaps_rollup" ]; then
                s_rss=$(awk '/^Rss:/ {rss+=$2} END {printf "%.0f", rss}' "/proc/$PID/smaps_rollup")
            else
                s_rss=$(awk '/^Rss:/ {rss+=$2} END {printf "%.0f", rss}' "/proc/$PID/smaps")
            fi

            # --- V8 RSS from smaps ---
            v8_rss=0
            if [ -f "/proc/$PID/smaps" ]; then
                v8_rss=$(awk '/^[0-9a-f]+-[0-9a-f]+/ {if (tolower($0) ~ /v8/) is_v8=1; else is_v8=0} /^Rss:/ {if (is_v8) sum+=$2} END {printf "%.0f", sum}' "/proc/$PID/smaps")
            fi

            # --- /proc/$PID/status ---
            t_rss=$(awk '/^VmRSS:/ {print $2}' "/proc/$PID/status" 2>/dev/null)

            # --- GPU Memory (Mali) ---
            g_rss=0
            if [ -f "/sys/kernel/debug/mali0/gpu_memory" ]; then
                # Sum up used_pages for the PID. Page size is 4KB.
                gpu_pages=$(awk -v pid="$PID" '$2 == pid {sum+=$3} END {print sum}' /sys/kernel/debug/mali0/gpu_memory 2>/dev/null)
                if [[ "$gpu_pages" =~ ^[0-9]+$ ]]; then
                    g_rss=$((gpu_pages * 4))
                fi
            fi

            # Record samples
            if [[ "$s_rss" =~ ^[0-9]+$ ]]; then smaps_rss_samples+=($s_rss); fi
            if [[ "$v8_rss" =~ ^[0-9]+$ ]]; then v8_rss_samples+=($v8_rss); fi
            if [[ "$t_rss" =~ ^[0-9]+$ ]]; then status_rss_samples+=($t_rss); fi
            gpu_rss_samples+=($g_rss)

            printf "[%3d/%3d] smaps: RSS %s MB | v8: %s MB | status: RSS %s MB | gpu: %s MB\n" \
                "$i" "$scenario_duration" "$(kb_to_mb $s_rss)" "$(kb_to_mb $v8_rss)" "$(kb_to_mb $t_rss)" "$(kb_to_mb $g_rss)"

            # Inject scroll event if requested
            if [ "$scenario_action" = "scroll" ]; then
                echo "Injecting scroll event: sendkey right"
                sendkey right > /dev/null 2>&1
            elif [ "$scenario_action" = "playback_scroll" ]; then
                if [ $i -eq 61 ] || [ $i -eq 62 ]; then
                    echo "Injecting key event: sendkey down"
                    sendkey down > /dev/null 2>&1
                elif [ $i -ge 63 ]; then
                    echo "Injecting scroll event: sendkey right"
                    sendkey right > /dev/null 2>&1
                fi
            fi

            sleep $INTERVAL
        done

        # Calculate metrics for this round
        r_smaps_median=$(calculate_median "${smaps_rss_samples[@]}")
        r_smaps_peak=$(calculate_peak "${smaps_rss_samples[@]}")
        r_v8_median=$(calculate_median "${v8_rss_samples[@]}")
        r_v8_peak=$(calculate_peak "${v8_rss_samples[@]}")
        r_status_median=$(calculate_median "${status_rss_samples[@]}")
        r_status_peak=$(calculate_peak "${status_rss_samples[@]}")
        r_gpu_median=$(calculate_median "${gpu_rss_samples[@]}")
        r_gpu_peak=$(calculate_peak "${gpu_rss_samples[@]}")

        round_smaps_median_rss+=($r_smaps_median)
        round_smaps_peak_rss+=($r_smaps_peak)
        round_v8_median_rss+=($r_v8_median)
        round_v8_peak_rss+=($r_v8_peak)
        round_status_median_rss+=($r_status_median)
        round_status_peak_rss+=($r_status_peak)
        round_gpu_median_rss+=($r_gpu_median)
        round_gpu_peak_rss+=($r_gpu_peak)

        echo ""
        echo "Round $round Results:"
        echo "  smaps  RSS - Peak: $(kb_to_mb $r_smaps_peak) MB, Median: $(kb_to_mb $r_smaps_median) MB"
        echo "  v8     RSS - Peak: $(kb_to_mb $r_v8_peak) MB, Median: $(kb_to_mb $r_v8_median) MB"
        echo "  status RSS - Peak: $(kb_to_mb $r_status_peak) MB, Median: $(kb_to_mb $r_status_median) MB"
        echo "  gpu    MEM - Peak: $(kb_to_mb $r_gpu_peak) MB, Median: $(kb_to_mb $r_gpu_median) MB"

        echo "Deactivating YouTube plugin (callsign: $CALLSIGN) to stop scenario..."
        deactivate_plugin

        # Wait for process to exit
        MAX_WAIT=20
        while [ -d "/proc/$PID" ] && [ $MAX_WAIT -gt 0 ]; do
            sleep 1
            ((MAX_WAIT--))
        done

        sleep 5 # Cool down
    done

    # Calculate averages for this scenario
    avg_s_med=$(calculate_average "${round_smaps_median_rss[@]}")
    avg_s_peak=$(calculate_average "${round_smaps_peak_rss[@]}")
    avg_v_med=$(calculate_average "${round_v8_median_rss[@]}")
    avg_v_peak=$(calculate_average "${round_v8_peak_rss[@]}")
    avg_t_med=$(calculate_average "${round_status_median_rss[@]}")
    avg_t_peak=$(calculate_average "${round_status_peak_rss[@]}")
    avg_g_med=$(calculate_average "${round_gpu_median_rss[@]}")
    avg_g_peak=$(calculate_average "${round_gpu_peak_rss[@]}")

    # Store for final report
    scenario_names_ordered+=("$scenario_name")
    scenario_rounds_ordered+=("$scenario_rounds")
    scenario_data_ordered+=("smaps_median:$avg_s_med;smaps_peak:$avg_s_peak;v8_median:$avg_v_med;v8_peak:$avg_v_peak;status_median:$avg_t_med;status_peak:$avg_t_peak;gpu_median:$avg_g_med;gpu_peak:$avg_g_peak")
done

echo ""
echo "=========================================================="
echo "FINAL TEST REPORT"
echo "=========================================================="

for ((i=0; i<${#scenario_names_ordered[@]}; i++)); do
    scenario_name="${scenario_names_ordered[$i]}"
    scenario_rounds="${scenario_rounds_ordered[$i]}"
    report_data="${scenario_data_ordered[$i]}"

    # Extract values from report_data string using awk for BusyBox compatibility
    avg_s_med=$(echo "$report_data" | awk -F'[;:]' '{print $2}')
    avg_s_peak=$(echo "$report_data" | awk -F'[;:]' '{print $4}')
    avg_v_med=$(echo "$report_data" | awk -F'[;:]' '{print $6}')
    avg_v_peak=$(echo "$report_data" | awk -F'[;:]' '{print $8}')
    avg_t_med=$(echo "$report_data" | awk -F'[;:]' '{print $10}')
    avg_t_peak=$(echo "$report_data" | awk -F'[;:]' '{print $12}')
    avg_g_med=$(echo "$report_data" | awk -F'[;:]' '{print $14}')
    avg_g_peak=$(echo "$report_data" | awk -F'[;:]' '{print $16}')

    echo "Scenario: $scenario_name ($scenario_rounds rounds)"
    printf "  Source: smaps\n"
    printf "    Average Median RSS: %s MB\n" "$(kb_to_mb $avg_s_med)"
    printf "    Average Peak RSS:   %s MB\n" "$(kb_to_mb $avg_s_peak)"
    printf "  Source: v8\n"
    printf "    Average Median RSS: %s MB\n" "$(kb_to_mb $avg_v_med)"
    printf "    Average Peak RSS:   %s MB\n" "$(kb_to_mb $avg_v_peak)"
    printf "  Source: status\n"
    printf "    Average Median RSS: %s MB\n" "$(kb_to_mb $avg_t_med)"
    printf "    Average Peak RSS:   %s MB\n" "$(kb_to_mb $avg_t_peak)"
    printf "  Source: gpu\n"
    printf "    Average Median RSS: %s MB\n" "$(kb_to_mb $avg_g_med)"
    printf "    Average Peak RSS:   %s MB\n" "$(kb_to_mb $avg_g_peak)"
    echo "----------------------------------------------------------"
done
echo "=========================================================="
echo "All results saved to $OUTPUT_FILE"
