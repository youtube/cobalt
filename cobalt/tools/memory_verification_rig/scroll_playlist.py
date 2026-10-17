#!/usr/bin/env python3
"""ADB-based playlist scroller for dynamic video hopping CUJ on Android TV."""
import subprocess
import sys
import time

if len(sys.argv) < 3:
  print("Usage: scroll_playlist.py <device> <duration>")
  sys.exit(1)

device = sys.argv[1]
duration = float(sys.argv[2])

print(f"Starting playlist video hopper on {device} for {duration}s...")


def send_key(key):
  subprocess.run(["adb", "-s", device, "shell", "input", "keyevent",
                  str(key)],
                 stdout=subprocess.DEVNULL,
                 stderr=subprocess.DEVNULL,
                 check=False)


start_time = time.time()
last_hop_time = start_time

while time.time() - start_time < duration:
  time.sleep(1)  # Poll every second
  elapsed = time.time() - start_time

  # Hop every 60 seconds
  if time.time() - last_hop_time >= 60.0:
    # Ensure we have at least 15 seconds left in the run to complete the hop
    if duration - elapsed >= 15.0:
      print(f"Hopping to new video (Elapsed: {int(elapsed)}s)...")
      for _ in range(3):
        send_key(20)  # DPAD_DOWN
        time.sleep(0.8)
      for _ in range(5):
        send_key(22)  # DPAD_RIGHT
        time.sleep(0.5)
      time.sleep(1.0)  # Let the horizontal scroll animation settle completely
      send_key(23)  # DPAD_CENTER
      last_hop_time = time.time()

print("Playlist video hopper finished.")
