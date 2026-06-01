#!/usr/bin/env python3
"""ADB-based UI scroller for Android TV."""
import subprocess
import sys
import time

if len(sys.argv) < 4:
  print("Usage: scroll_adb.py <device> <duration> <interval>")
  sys.exit(1)

device = sys.argv[1]
duration = float(sys.argv[2])
interval = float(sys.argv[3])

print(f"Starting ADB scroller on {device} for {duration}s...")
start_time = time.time()
while time.time() - start_time < duration:
  # 20 is KEYCODE_DPAD_DOWN
  subprocess.run(["adb", "-s", device, "shell", "input", "keyevent", "20"],
                 stdout=subprocess.DEVNULL,
                 stderr=subprocess.DEVNULL,
                 check=False)
  time.sleep(interval)
print("ADB scroller finished.")
