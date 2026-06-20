#!/usr/bin/env python3
"""
Cobalt UI Stress Test Automation Script (Pure Key Injector)
Simulates a complex user journey (video playback, recommendations browsing,
home screen scrolling, refresh, sidebar navigation, and new video launch)
in a continuous loop for a specified duration.

Assumes the Cobalt app is already running on the target device.
"""

import argparse
import time
import subprocess
import sys

def send_key(shell, key_code, delay=0.1):
    """Sends a keyevent to the persistent shell and sleeps."""
    shell.stdin.write(f"input keyevent {key_code}\n")
    shell.stdin.flush()
    time.sleep(delay)

def run_stress_test(serial, duration_seconds, play_delay):
    device_arg = ["-s", serial] if serial else []
    
    # Launch Cobalt with the target video (Test script is responsible for launch!)
    video_url = "https://www.youtube.com/tv?v=nxcKFjP5QrE"
    print(f"Launching Cobalt with video: {video_url}")
    subprocess.run(["adb"] + device_arg + ["shell", "am", "start", "-n", "dev.cobalt.coat/dev.cobalt.app.MainActivity", "-d", video_url])
    
    # Wait for app to boot and video to start playing
    print("Waiting 15 seconds for Cobalt to boot and start video playback...")
    time.sleep(15)

    # Open a persistent shell for ultra-fast key injection (bypasses adb startup overhead)
    shell_cmd = ["adb"] + device_arg + ["shell"]
    print(f"Opening persistent shell: {' '.join(shell_cmd)}")
    shell = subprocess.Popen(shell_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    start_time = time.time()
    cycle_count = 0
    
    # Android Keycodes
    DOWN = 20
    BACK = 4
    SELECT = 23
    
    try:
        while time.time() - start_time < duration_seconds:
            cycle_count += 1
            elapsed = int(time.time() - start_time)
            print(f"\n🚀 Starting Cycle {cycle_count} (Elapsed: {elapsed}s / {duration_seconds}s)")
            
            # Step 1 & 2: Press DOWN 30 times (pop up and scroll recommendation list)
            print("  -> Pressing DOWN 30 times (scrolling recommendations)...")
            for _ in range(30):
                send_key(shell, DOWN, 0.1)
                
            # Step 3: Press BACK (dismiss recommendations, video continues playing)
            print("  -> Pressing BACK (dismiss recommendations)...")
            send_key(shell, BACK, 0.1)
            
            # Step 4: Press BACK (go to Home Screen)
            print("  -> Pressing BACK (going to Home Screen)...")
            send_key(shell, BACK, 0.1)
            
            # Step 5: Press DOWN 30 times (scroll Home screen to the end - focus on refresh)
            print("  -> Pressing DOWN 30 times (scrolling Home)...")
            for _ in range(30):
                send_key(shell, DOWN, 0.1)
                
            # Step 6: Press SELECT (click refresh button)
            print("  -> Pressing SELECT (click refresh)...")
            send_key(shell, SELECT, 0.1)
            
            # Step 7: Wait 5 seconds for screen to refresh
            print("  -> Waiting 5 seconds for screen to refresh...")
            time.sleep(5)
            
            # Step 8: Press BACK (go to side bar)
            print("  -> Pressing BACK (go to sidebar)...")
            send_key(shell, BACK, 0.1)
            
            # Step 9: Press DOWN 10 times (scroll down side bar - 1.0s delay!)
            print("  -> Pressing DOWN 10 times (scrolling sidebar)...")
            for _ in range(10):
                send_key(shell, DOWN, 1.0)
                
            # Step 10: Press BACK (focus moves to HOME in side bar)
            print("  -> Pressing BACK (focus to HOME in sidebar)...")
            send_key(shell, BACK, 0.1)
            
            # Step 11: Press SELECT (focus moves to first video on Home screen)
            print("  -> Pressing SELECT (focus to first video)...")
            send_key(shell, SELECT, 0.1)
            
            # Step 12: Press SELECT (play the video)
            print("  -> Pressing SELECT (start video playback)...")
            send_key(shell, SELECT, 0.1)
            
            # Step 13: Wait for video to buffer and play cleanly before starting next cycle
            print(f"  -> Waiting {play_delay} seconds for video to play cleanly...")
            time.sleep(play_delay)
            
    except KeyboardInterrupt:
        print("\n[INFO] Test interrupted by user (Ctrl+C).")
    finally:
        print("Closing persistent shell session...")
        try:
            shell.stdin.write("exit\n")
            shell.stdin.flush()
        except Exception:
            pass
        shell.terminate()
        duration_run = int(time.time() - start_time)
        print(f"\n🎉 Test Finished! Completed {cycle_count} full cycles in {duration_run}s.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Cobalt UI Stress Test Automation")
    parser.add_argument("-s", "--serial", type=str, help="Device serial number")
    parser.add_argument("-d", "--duration", type=int, default=300, help="Duration of the test in seconds (default: 300)")
    parser.add_argument("-p", "--play-delay", type=int, default=10, help="Delay in seconds after starting video playback (default: 10)")
    args = parser.parse_args()
    
    run_stress_test(args.serial, args.duration, args.play_delay)
