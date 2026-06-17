#!/bin/bash -ex

DEVICE="localhost:38473"
PACKAGE="dev.cobalt.coat"
ACTIVITY="dev.cobalt.app.MainActivity"
URL="https://www.youtube.com/tv?v=LXb3EKWsInQ"

function adb_cmd() {
  adb -s "$DEVICE" "$@"
}

echo "=== 0. Force-stopping existing Cobalt app ==="
adb_cmd shell am force-stop "$PACKAGE"

echo "=== 1. Cleaning adb logcat ==="
adb_cmd logcat -c

echo "=== 2. Starting background adb logcat capture ==="
adb_cmd logcat -v threadtime | tee log.adb.txt &
LOGCAT_PID=$!

echo "=== 3. Launching app playing video ==="
adb_cmd shell am start -n "$PACKAGE/$ACTIVITY" -d "$URL"

echo "=== 4. Sampling Foreground memory (30s & 60s) ==="
sleep 30
adb_cmd shell dumpsys meminfo "$PACKAGE" > dumpsys_30s.txt

sleep 30
adb_cmd shell dumpsys meminfo "$PACKAGE" > dumpsys_60s.txt

echo "=== 5. Sending app to Background (HOME button pressed) ==="
adb_cmd shell input keyevent KEYCODE_HOME

echo "=== 6. Sampling Background memory (90s & 120s) ==="
sleep 30
adb_cmd shell dumpsys meminfo "$PACKAGE" > dumpsys_90s.txt

sleep 30
adb_cmd shell dumpsys meminfo "$PACKAGE" > dumpsys_120s.txt

echo "=== 7. Stopping app & capture ==="
adb_cmd shell am force-stop "$PACKAGE"
kill "$LOGCAT_PID" || true

echo "=== 8. Post-processing whole dumpsys files ==="
rm -f metrics.txt
for t in 30 60 90 120; do
  echo "=== t=${t}s Telemetry ===" >> metrics.txt
  grep -E "TOTAL PSS|Native Heap|Graphics|Unknown|Private Other" "dumpsys_${t}s.txt" >> metrics.txt || true
  echo "" >> metrics.txt
done

cat metrics.txt
