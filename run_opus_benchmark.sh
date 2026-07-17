#!/bin/bash
set -e

# Change directory to the script's directory (workspace root)
cd "$(dirname "$0")"

echo "=== Building opus_benchmark ==="
env -u ANTIGRAVITY_AGENT autoninja -C out/android-arm_qa/ third_party/opus:opus_benchmark

echo "=== Pushing to device ==="
adb push out/android-arm_qa/opus_benchmark /data/local/tmp/

echo "=== Setting permissions ==="
adb shell chmod +x /data/local/tmp/opus_benchmark

echo "=== Running benchmark ==="
adb shell /data/local/tmp/opus_benchmark
