#!/bin/bash

# Define your package and PID
PKG_NAME="dev.cobalt.coat"

PID=$(adb shell pidof -s $PKG_NAME)

if [ -z "$PID" ]; then
    echo "Error: Process $PKG_NAME not found."
    exit 1
fi

echo "Timestamp,PSS,RSS,Graphics" > stats.txt
echo "Logging memory data for PID $PID..."

while true; do
    # Get the meminfo dump for the specific PID
    DUMP=$(adb shell dumpsys meminfo $PID)

    # Extract metrics using awk
    PSS=$(echo "$DUMP" | grep "TOTAL PSS:" | awk '{print $3}')
    RSS=$(echo "$DUMP" | grep "TOTAL RSS:" | awk '{print $6}')
    GPU=$(echo "$DUMP" | grep "Graphics:" | awk '{print $2}')
    TIMESTAMP=$(date +%H:%M:%S)

    echo "$TIMESTAMP,$PSS,$RSS,$GPU" >> stats.txt
    echo "[$TIMESTAMP] PSS: ${PSS}KB | RSS: ${RSS}KB | GPU: ${GPU}KB"
    
    sleep 1
done