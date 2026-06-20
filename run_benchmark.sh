#!/usr/bin/env bash
#
# Cobalt C26 vs C27 Memory Benchmarking & Comparison Automator
#
# This script automates the complete end-to-end memory benchmarking process:
# 1. Deploys C26, launches it with a video, captures background logcat, and waits.
# 2. Deploys C27, launches it with the same video, captures background logcat, and waits.
# 3. Parses both logcats, exports CSVs, and generates PNG plots using parse_and_plot_memory.py.
# 4. Performs a mathematical steady-state comparison and prints an instant report!
#
# Usage:
#   ./run_benchmark.sh --duration 300 --serial 52221HFAG0KQT6
#

set -e

# --- Terminal Colors ---
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# --- Defaults ---
DURATION=300
SERIAL=""
C26_APK="/usr/local/google/home/kjyoun/apks/Cobalt.c26.metrics.apk"
C27_APK="/usr/local/google/home/kjyoun/apks/Cobalt.c27.metrics.apk"
VIDEO_URL="https://www.youtube.com/tv?v=nxcKFjP5QrE"
STRESS_SCRIPT=""

# --- Argument Parsing ---
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d|--duration) DURATION="$2"; shift ;;
        -s|--serial) SERIAL="$2"; shift ;;
        --script) STRESS_SCRIPT="$2"; shift ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -d, --duration <seconds>   Duration to run each test (default: 300)"
            echo "  -s, --serial <device_id>   ADB device serial number (optional)"
            echo "  --script <path_to_script>  Path to active UI automation script (optional)"
            exit 0
            ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# --- Setup ADB command prefix ---
ADB_CMD="adb"
if [ -n "$SERIAL" ]; then
    ADB_CMD="adb -s $SERIAL"
    echo -e "${GREEN}[SETUP] Targeting device serial: $SERIAL${NC}"
else
    # Auto-detect if only one device is connected
    DEVICES_COUNT=$(adb devices | grep -c -w "device" || true)
    if [ "$DEVICES_COUNT" -eq 1 ]; then
        ACTIVE_DEV=$(adb devices | grep -w "device" | awk '{print $1}')
        ADB_CMD="adb -s $ACTIVE_DEV"
        echo -e "${GREEN}[SETUP] Auto-detected active device: $ACTIVE_DEV${NC}"
    elif [ "$DEVICES_COUNT" -gt 1 ]; then
        echo -e "${RED}[ERROR] Multiple devices connected. Please specify serial using -s or --serial.${NC}"
        adb devices
        exit 1
    else
        echo -e "${RED}[ERROR] No ADB devices connected!${NC}"
        exit 1
    fi
fi

TIMESTAMP=$(date +"%m%d_%H%M")
RESULT_DIR="kjyoun.gemini/memory-pressure/test-run-${TIMESTAMP}"
mkdir -p "$RESULT_DIR"
echo -e "${GREEN}[SETUP] Created results directory: $RESULT_DIR${NC}"

C26_LOG="${RESULT_DIR}/log.adb.c26_${TIMESTAMP}.txt"
C27_LOG="${RESULT_DIR}/log.adb.c27_${TIMESTAMP}.txt"

C26_CSV="${RESULT_DIR}/cobalt_c26_${TIMESTAMP}.csv"
C27_CSV="${RESULT_DIR}/cobalt_c27_${TIMESTAMP}.csv"
C26_PNG="${RESULT_DIR}/cobalt_c26_${TIMESTAMP}.png"
C27_PNG="${RESULT_DIR}/cobalt_c27_${TIMESTAMP}.png"

# --- Helper Function: Run Test Cycle ---
run_test_cycle() {
    local branch_name=$1
    local apk_path=$2
    local log_file=$3

    echo -e "\n================================================================="
    echo -e "${GREEN}🚀 STARTING TEST CYCLE: $branch_name${NC}"
    echo -e "================================================================="

    echo -e "${YELLOW}[1/6] Force-stopping any running Cobalt instance...${NC}"
    $ADB_CMD shell am force-stop dev.cobalt.coat || true
    sleep 2

    echo -e "${YELLOW}[2/6] Installing $branch_name APK: $apk_path ...${NC}"
    if [ ! -f "$apk_path" ]; then
        echo -e "${RED}[ERROR] APK file not found at $apk_path !${NC}"
        exit 1
    fi
    $ADB_CMD install -r "$apk_path"
    sleep 3

    echo -e "${YELLOW}[3/6] Clearing device logcat buffers...${NC}"
    $ADB_CMD logcat -c
    sleep 1

    if [ -z "$STRESS_SCRIPT" ]; then
        echo -e "${YELLOW}[4/6] Launching Cobalt with video: $VIDEO_URL (Idle Mode)...${NC}"
        $ADB_CMD shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity -d "$VIDEO_URL"
        sleep 5
    else
        echo -e "${YELLOW}[4/6] Skipping launch in runner. Stress script will handle launch.${NC}"
    fi

    echo -e "${GREEN}[5/6] Starting background logcat capture to: $log_file ...${NC}"
    $ADB_CMD logcat -v time > "$log_file" &
    local logcat_pid=$!

    echo -e "${GREEN}[6/6] Capture active. Waiting for $DURATION seconds (steady state profiling)...${NC}"
    if [ -n "$STRESS_SCRIPT" ]; then
        if [ -f "$STRESS_SCRIPT" ]; then
            echo -e "${YELLOW}[STRESS] Running active UI stress test: $STRESS_SCRIPT ...${NC}"
            local stress_args=("--duration" "$DURATION")
            if [ -n "$SERIAL" ]; then
                stress_args+=("--serial" "$SERIAL")
            fi
            python3 "$STRESS_SCRIPT" "${stress_args[@]}"
        else
            echo -e "${RED}[ERROR] Stress script '$STRESS_SCRIPT' not found! Exiting to prevent invalid benchmark.${NC}"
            exit 1
        fi
    else
        echo -e "${YELLOW}[IDLE] Running static playback (no stress script specified). Sleeping for $DURATION seconds...${NC}"
        local elapsed=0
        while [ $elapsed -lt "$DURATION" ]; do
            sleep 10
            elapsed=$((elapsed + 10))
            echo -e "  -> Progress: $elapsed / $DURATION seconds elapsed..."
        done
    fi

    echo -e "${GREEN}[7/7] Capturing final dumpsys meminfo Ground Truth...${NC}"
    $ADB_CMD shell dumpsys meminfo dev.cobalt.coat > "${RESULT_DIR}/cobalt_${branch_name}_final_dumpsys_${TIMESTAMP}.txt" || true
    sleep 2

    echo -e "${YELLOW}[CLEANUP] Stopping logcat capture (PID: $logcat_pid)...${NC}"
    kill "$logcat_pid" || true
    sleep 2

    echo -e "${YELLOW}[CLEANUP] Force-stopping Cobalt...${NC}"
    $ADB_CMD shell am force-stop dev.cobalt.coat || true
    echo -e "${GREEN}✓ Completed Test Cycle: $branch_name${NC}"
}

# --- Execute Benchmark Runs ---
run_test_cycle "C26" "$C26_APK" "$C26_LOG"
run_test_cycle "C27" "$C27_APK" "$C27_LOG"

# --- Post-Processing: Parse & Plot ---
echo -e "\n================================================================="
echo -e "${GREEN}📊 POST-PROCESSING: PARSING & PLOTTING METRICS${NC}"
echo -e "================================================================="

if [ ! -f "./parse_and_plot_memory.py" ]; then
    echo -e "${RED}[ERROR] Parser script './parse_and_plot_memory.py' not found in current directory!${NC}"
    exit 1
fi

echo -e "${YELLOW}Parsing C26 logs...${NC}"
python3 parse_and_plot_memory.py -l "$C26_LOG" -c "$C26_CSV" -p "$C26_PNG"

echo -e "${YELLOW}Parsing C27 logs...${NC}"
python3 parse_and_plot_memory.py -l "$C27_LOG" -c "$C27_CSV" -p "$C27_PNG"

# --- Run Comparison & Generate Hierarchical Report ---
echo -e "\n================================================================="
echo -e "${GREEN}📊 AUTOMATED STEADY-STATE COMPARISON REPORT${NC}"
echo -e "================================================================="

if [ -f "./compare_runs.py" ]; then
    python3 ./compare_runs.py --control "$C26_CSV" --treatment "$C27_CSV" | tee "${RESULT_DIR}/analysis_report.md"
else
    echo -e "${YELLOW}[WARNING] './compare_runs.py' not found! Skipping hierarchical report.${NC}"
fi

echo -e "\n================================================================="
echo -e "${GREEN}🎉 BENCHMARK COMPLETE! ALL ARTIFACTS GENERATED SUCCESSFULLY!${NC}"
echo -e "================================================================="
echo -e "📂 Results Folder:  $RESULT_DIR"
echo -e "📂 C26 Logcat:      $C26_LOG"
echo -e "📂 C27 Logcat:      $C27_LOG"
echo -e "📂 C26 Spreadsheet: $C26_CSV"
echo -e "📂 C27 Spreadsheet: $C27_CSV"
echo -e "🖼️ C26 Memory Plot: $C26_PNG"
echo -e "🖼️ C27 Memory Plot: $C27_PNG"
echo -e "📄 Analysis Report: $RESULT_DIR/analysis_report.md"
echo -e "================================================================="
