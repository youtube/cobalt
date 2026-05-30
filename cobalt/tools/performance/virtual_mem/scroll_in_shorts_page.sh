#!/bin/bash

# Define your package and PID
PKG_NAME="dev.cobalt.coat"
TARGET_PID=$(adb shell pidof -s $PKG_NAME)

if [ -z "$TARGET_PID" ]; then
    echo "App not running. Please launch it first."
    exit 1
fi

echo "Detailed Stats tracking for PID: $TARGET_PID"

# THE MAIN LOOP
while true; do
    echo "Entering Shorts..."
    # Tap Enter twice (Enter -> Pause/Focus)
    adb shell input keyevent 23
    adb shell input keyevent 23

    # --- THE FIXED SCROLLING LOOP ---
    count=0
    # This ensures exactly 1200 iterations
    while [ $count -lt 1200 ]; do
        
        # 1. Scroll Down
        adb shell input keyevent 20
        
        # 2. Capture Stats
        # We append the current time (seconds) to make the CSV easier to parse later
        timestamp=$(date +%s)
        adb shell cat /proc/$TARGET_PID/stat | awk -v time=$timestamp '{print time, $0}' >> stats.txt

        # 3. Increment Counter
        count=$((count+1))
        
        # 4. Sleep to match scrolling speed (adjust as needed)
        sleep 0.8
    done
    # -------------------------------

    echo "1200 Scrolls finished. Exiting to Home..."
    adb shell input keyevent 4
    
    # Wait longer at the home screen to see if memory drops
    sleep 5
done
