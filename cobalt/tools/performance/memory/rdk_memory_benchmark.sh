#!/bin/bash

# Configuration
readonly PROCESS_NAME="loader_app"
readonly INTERVAL=1
readonly OUTPUT_FILE="memory_test_results.txt"

# Initialize output file
echo "Memory Test Started at $(date)" > "$OUTPUT_FILE"

# Redirect stdout and stderr to both terminal and output file
exec > >(tee -a "$OUTPUT_FILE") 2>&1

# Define scenarios: name|command|duration|rounds
# If rounds is omitted, it defaults to 1.
SCENARIOS=(
    "Default Page|./out_cobalt/loader_app|60|3"
    "About Blank|./out_cobalt/loader_app --url=\"about:blank\"|30|1"
    "4K Video Playback|./out_cobalt/loader_app --url=\"https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ\"|180|3"
    "1080p Video Playback|./out_cobalt/loader_app --url=\"https://www.youtube.com/tv#/watch?v=ou0cmY8Fqd0\"|180|3"
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
    if [ -z "$kb" ] || [ "$kb" -eq 0 ]; then echo "0.00"; else
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

# Store final results for each scenario using parallel arrays for compatibility
scenario_names_ordered=()
scenario_data_ordered=()
scenario_rounds_ordered=()

for scenario_info in "${SCENARIOS[@]}"; do
    IFS='|' read -r scenario_name scenario_command scenario_duration scenario_rounds <<< "$scenario_info"

    # Default to 1 round if not specified
    if [ -z "$scenario_rounds" ]; then
        scenario_rounds=1
    fi

    echo ""
    echo "##########################################################"
    echo "SCENARIO: $scenario_name"
    echo "COMMAND:  $scenario_command"
    echo "DURATION: $scenario_duration seconds"
    echo "ROUNDS:   $scenario_rounds"
    echo "##########################################################"

    round_smaps_median_rss=()
    round_smaps_peak_rss=()
    round_status_median_rss=()
    round_status_peak_rss=()
    round_gpu_median_rss=()
    round_gpu_peak_rss=()

    for ((round=1; round<=$scenario_rounds; round++)); do
        echo "=========================================================="
        echo "Starting Round $round/$scenario_rounds"
        echo "Launching $scenario_command..."

        # Evaluate command to handle quotes correctly
        eval "$scenario_command > /dev/null 2>&1 &"
        LAUNCH_PID=$!

        # Wait for the process to be available
        sleep 5 # Give it a bit more time to settle
        PID=$(pgrep -n "$PROCESS_NAME")

        if [ -z "$PID" ]; then
            echo "Error: Process '$PROCESS_NAME' not found after launch."
            kill $LAUNCH_PID 2>/dev/null
            continue
        fi

        echo "Target PID: $PID"
        echo "Collecting samples for $scenario_duration seconds..."

        smaps_rss_samples=()
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
            if [[ "$t_rss" =~ ^[0-9]+$ ]]; then status_rss_samples+=($t_rss); fi
            gpu_rss_samples+=($g_rss)

            printf "[%3d/%3d] smaps: RSS %s MB | status: RSS %s MB | gpu: %s MB\n" \
                "$i" "$scenario_duration" "$(kb_to_mb $s_rss)" "$(kb_to_mb $t_rss)" "$(kb_to_mb $g_rss)"

            sleep $INTERVAL
        done

        # Calculate metrics for this round
        r_smaps_median=$(calculate_median "${smaps_rss_samples[@]}")
        r_smaps_peak=$(calculate_peak "${smaps_rss_samples[@]}")
        r_status_median=$(calculate_median "${status_rss_samples[@]}")
        r_status_peak=$(calculate_peak "${status_rss_samples[@]}")
        r_gpu_median=$(calculate_median "${gpu_rss_samples[@]}")
        r_gpu_peak=$(calculate_peak "${gpu_rss_samples[@]}")

        round_smaps_median_rss+=($r_smaps_median)
        round_smaps_peak_rss+=($r_smaps_peak)
        round_status_median_rss+=($r_status_median)
        round_status_peak_rss+=($r_status_peak)
        round_gpu_median_rss+=($r_gpu_median)
        round_gpu_peak_rss+=($r_gpu_peak)

        echo ""
        echo "Round $round Results:"
        echo "  smaps  RSS - Peak: $(kb_to_mb $r_smaps_peak) MB, Median: $(kb_to_mb $r_smaps_median) MB"
        echo "  status RSS - Peak: $(kb_to_mb $r_status_peak) MB, Median: $(kb_to_mb $r_status_median) MB"
        echo "  gpu    MEM - Peak: $(kb_to_mb $r_gpu_peak) MB, Median: $(kb_to_mb $r_gpu_median) MB"

        echo "Killing processes $PID and $LAUNCH_PID..."
        kill $PID $LAUNCH_PID 2>/dev/null

        # Wait for process to exit
        MAX_WAIT=20
        while [ -d "/proc/$PID" ] && [ $MAX_WAIT -gt 0 ]; do
            sleep 1
            ((MAX_WAIT--))
        done
        if [ -d "/proc/$PID" ]; then
            echo "Forcing kill -9 on $PID and $LAUNCH_PID..."
            kill -9 $PID $LAUNCH_PID 2>/dev/null
        fi

        sleep 5 # Cool down
    done

    # Calculate averages for this scenario
    avg_s_med=$(calculate_average "${round_smaps_median_rss[@]}")
    avg_s_peak=$(calculate_average "${round_smaps_peak_rss[@]}")
    avg_t_med=$(calculate_average "${round_status_median_rss[@]}")
    avg_t_peak=$(calculate_average "${round_status_peak_rss[@]}")
    avg_g_med=$(calculate_average "${round_gpu_median_rss[@]}")
    avg_g_peak=$(calculate_average "${round_gpu_peak_rss[@]}")

    # Store for final report
    scenario_names_ordered+=("$scenario_name")
    scenario_rounds_ordered+=("$scenario_rounds")
    scenario_data_ordered+=("smaps_median:$avg_s_med;smaps_peak:$avg_s_peak;status_median:$avg_t_med;status_peak:$avg_t_peak;gpu_median:$avg_g_med;gpu_peak:$avg_g_peak")
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
    avg_t_med=$(echo "$report_data" | awk -F'[;:]' '{print $6}')
    avg_t_peak=$(echo "$report_data" | awk -F'[;:]' '{print $8}')
    avg_g_med=$(echo "$report_data" | awk -F'[;:]' '{print $10}')
    avg_g_peak=$(echo "$report_data" | awk -F'[;:]' '{print $12}')

    echo "Scenario: $scenario_name ($scenario_rounds rounds)"
    printf "  Source: smaps\n"
    printf "    Average Median RSS: %s MB\n" "$(kb_to_mb $avg_s_med)"
    printf "    Average Peak RSS:   %s MB\n" "$(kb_to_mb $avg_s_peak)"
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
