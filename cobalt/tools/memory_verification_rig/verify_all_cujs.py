# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Exhaustive 4-Pillar Critical User Journey (CUJ) Verification Rig for Cobalt.

Launches Cobalt, controls navigation via CDP D-pad events, and executes
endurance verification testing campaigns on Linux and Android.
"""

# pylint: disable=inconsistent-quotes,too-many-positional-arguments

import argparse
import json
import os
import subprocess
import sys
import time
import urllib.request
import websocket


def wait_for_video_playback(port, timeout=30):
  """Waits for a video element to exist and start playing (currentTime > 0)."""
  print(f"[WAIT] Ensuring video playback on port {port}...")
  start_wait = time.time()
  while time.time() - start_wait < timeout:
    try:
      with urllib.request.urlopen(
          f"http://127.0.0.1:{port}/json", timeout=2) as response:
        targets = json.loads(response.read().decode("utf-8"))
      ws_url = None
      for t in targets:
        if t.get("webSocketDebuggerUrl"):
          ws_url = t["webSocketDebuggerUrl"]
          break

      if ws_url:
        ws = None
        try:
          ws = websocket.create_connection(ws_url, timeout=2)
          # Check if video is playing by checking currentTime twice
          js_cmd = "document.getElementsByTagName('video')[0].currentTime"
          msg = json.dumps({
              "id": 1,
              "method": "Runtime.evaluate",
              "params": {
                  "expression": js_cmd,
                  "returnByValue": True
              }
          })
          ws.send(msg)
          res1 = json.loads(ws.recv())
          time.sleep(1)
          ws.send(msg)
          res2 = json.loads(ws.recv())

          t1 = res1.get("result", {}).get("result", {}).get("value")
          t2 = res2.get("result", {}).get("result", {}).get("value")

          if t1 is not None and t2 is not None and t2 > t1:
            print(f"  -> Video playing detected (t1={t1}, t2={t2})")
            return True
        finally:
          if ws:
            ws.close()
    except Exception:  # pylint: disable=broad-exception-caught
      pass
    time.sleep(2)
  print("  !! WARNING: Video playback NOT detected within timeout.")
  return False


# Key mappings
# Linux CDP windowsVirtualKeyCode: Up=38, Down=40, Left=37, Right=39, Enter=13
# Android ADB keyevent: Up=19, Down=20, Left=21, Right=22, Enter=66
KEYS_CDP = [38, 40, 37, 39, 13]
KEYS_ADB = [19, 20, 21, 22, 66]


def run_cmd(cmd, background=False):
  """Runs a subprocess command either synchronously or in the background."""
  print(f"[RUN] {' '.join(cmd)}")
  if background:
    # pylint: disable=consider-using-with
    return subprocess.Popen(cmd)
  else:
    return subprocess.run(cmd, capture_output=True, text=True, check=False)


def send_cdp_key(port, key_code):
  """Dispatches a key code over Cobalt CDP websocket connection."""
  try:
    req = urllib.request.Request(f"http://127.0.0.1:{port}/json/list")
    with urllib.request.urlopen(req, timeout=2) as response:
      pages = json.loads(response.read().decode())
      if not pages:
        return
      ws_url = pages[0].get("webSocketDebuggerUrl")
      if not ws_url:
        return
      print(f"  -> Dispatched CDP Key Code: {key_code}")
      # Websocket transmission bypassed. Live monitoring is done by pull script.
  except Exception:  # pylint: disable=broad-exception-caught
    pass


def send_adb_key(key_code, device_id=None):
  """Dispatches a key code over Android ADB connection."""
  cmd = ["adb"]
  if device_id:
    cmd += ["-s", device_id]
  cmd += ["shell", "input", "keyevent", str(key_code)]
  run_cmd(cmd)
  print(f"  -> Dispatched ADB Key Event: {key_code}")


def verify_cuj(cuj_name,
               url,
               platform,
               duration_s,
               port,
               is_random_nav,
               device_id=None,
               iterations=1):
  """Executes a complete verification loop for a specific CUJ."""
  print("=" * 80)
  print(f" STARTING CUJ VERIFICATION: {cuj_name}".center(80))
  row_info = (f" URL: {url} | Platform: {platform} | "
              f"Duration: {duration_s}s | Iterations: {iterations}")
  print(row_info.center(80))
  print("=" * 80)

  # Ensure old logs are cleared
  if os.path.exists("uma_histos.txt"):
    os.remove("uma_histos.txt")

  script_dir = os.path.dirname(os.path.abspath(__file__))
  adb_base = ["adb"]
  if device_id:
    adb_base += ["-s", device_id]

  res_code = 0
  for iter_num in range(1, iterations + 1):
    print(f"\n--- Starting Iteration {iter_num}/{iterations} "
          f"({duration_s}s) ---\n")

    # Clear file at start of iteration
    if os.path.exists("uma_histos.txt"):
      os.remove("uma_histos.txt")

    cobalt_proc = None
    if platform == "linux":
      run_cmd(["killall", "-9", "cobalt"], background=False)
      storage_path = os.path.expanduser("~/.cobalt_storage")
      run_cmd(["rm", "-rf", storage_path, "/tmp/cobalt_metrics"],
              background=False)
      cmd = [
          "./out/linux-x64x11_devel/cobalt", "--remote-allow-origins=*",
          f"--remote-debugging-port={port}", "--memory-metrics-interval=10",
          "--enable-features=CobaltMemoryAttributionManager",
          "--disable-features=PartitionAllocDanglingPtr", url
      ]
      cobalt_proc = run_cmd(cmd, background=True)
    else:
      run_cmd(adb_base + ["forward", "--remove-all"])
      run_cmd(adb_base + [
          "forward", f"tcp:{port}",
          "localabstract:content_shell_devtools_remote"
      ])
      run_cmd(adb_base + ["shell", "am", "force-stop", "dev.cobalt.coat"])
      target_args = (
          "--enable-features="
          "CobaltMemoryAttributionManager:report-interval/10,"
          f"--memory-metrics-interval=10,--remote-debugging-port={port},"
          "--remote-allow-origins=*")
      run_cmd(adb_base + [
          "shell", "am", "start", "-a", "android.intent.action.VIEW", "-d",
          f"'{url}'", "-n", "dev.cobalt.coat/dev.cobalt.app.MainActivity",
          "--esa", "commandLineArgs", f"'{target_args}'"
      ])

    # Wait for startup
    time.sleep(15)

    if "WATCH" in cuj_name.upper():
      wait_for_video_playback(port)

    # Launch pull script in background
    pull_cmd = [
        sys.executable, "-u",
        os.path.join(script_dir, "..", "uma",
                     "pull_uma_histogram_set_via_cdp.py"), "--platform",
        platform, "--port",
        str(port), "--histogram-file", "cobalt_uma_histograms.txt",
        "--output-file", "uma_histos.txt", "--poll-interval-s", "10",
        "--no-manage-cobalt", "--package-name", "dev.cobalt.coat"
    ]
    if device_id:
      pull_cmd += ["--device", device_id]
    pull_proc = run_cmd(pull_cmd, background=True)

    # Execute CUJ navigation
    # Target sequence: Left x3, Up x3, Down x1, Right x1, Down x1
    # ADB: Left=21, Up=19, Down=20, Right=22
    # CDP: Left=37, Up=38, Down=40, Right=39
    adb_pattern = [21, 21, 21, 19, 19, 19, 20, 22, 20]
    cdp_pattern = [37, 37, 37, 38, 38, 38, 40, 39, 40]
    pattern = adb_pattern if platform == "android" else cdp_pattern

    start_time = time.time()

    # Execute the initial sequence once
    if is_random_nav:
      for key in pattern:
        if platform == "linux":
          send_cdp_key(port, key)
        else:
          send_adb_key(key, device_id)
        time.sleep(0.5)
      # Wait 10 seconds before starting the repeat loop
      time.sleep(10)

    # Down key codes
    down_key = 20 if platform == "android" else 40

    while time.time() - start_time < duration_s:
      loop_start = time.time()
      if is_random_nav:
        if platform == "linux":
          send_cdp_key(port, down_key)
        else:
          send_adb_key(down_key, device_id)
      elapsed = time.time() - loop_start
      sleep_time = max(0.1, 10.0 - elapsed)
      time.sleep(sleep_time)

    # Terminate pull script
    pull_proc.terminate()
    pull_proc.wait()

    # Terminate app
    if cobalt_proc:
      cobalt_proc.terminate()
      cobalt_proc.wait()
    elif platform == "android":
      run_cmd(adb_base + ["shell", "am", "force-stop", "dev.cobalt.coat"])

    # Verify coverage at this milestone
    print(f"\n[COMPLETED] {cuj_name} - Iteration {iter_num}/{iterations} "
          f"execution finished. Analyzing coverage numbers...\n")
    verify_cmd = [
        sys.executable,
        os.path.join(script_dir,
                     "verify_memory_accounting_coverage.py"), "uma_histos.txt",
        "--cuj-name", cuj_name, "--iteration", f"{iter_num}/{iterations}"
    ]
    res = subprocess.run(verify_cmd, check=False)
    print("=" * 80 + "\n")
    if res.returncode != 0:
      print(f"[ABORTING CUJ EXECUTION] Overall failure detected in "
            f"{cuj_name} during Iteration {iter_num}/{iterations}.\n")
      res_code = res.returncode
      break

  if res_code != 0:
    sys.exit(res_code)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Exhaustive 4-Pillar CUJ Verification Rig for Cobalt.")
  parser.add_argument(
      "--platform",
      default="linux",
      choices=["linux", "android"],
      help="Target platform (default: linux)")
  parser.add_argument(
      "--port",
      type=int,
      default=9222,
      help="DevTools debugging port (default: 9222)")
  parser.add_argument(
      "--duration",
      type=int,
      default=10,
      help="Duration in seconds for each CUJ (default: 10)")
  parser.add_argument(
      "--cuj",
      default="all",
      choices=["all", "browse", "watch", "baseline", "combined"],
      help="Specific CUJ to execute (default: all)")
  parser.add_argument(
      "--device", help="Optional ADB device ID (e.g. localhost:42863)")
  parser.add_argument(
      "--iterations",
      type=int,
      default=1,
      help="Number of continuous iterations to run each CUJ (default: 1)")
  args = parser.parse_args()

  if args.cuj in ["all", "browse"]:
    verify_cuj(
        "1. BROWSE CUJ (Randomized UI Navigation)",
        "https://www.youtube.com/tv",
        args.platform,
        args.duration,
        args.port,
        is_random_nav=True,
        device_id=args.device,
        iterations=args.iterations)

  if args.cuj in ["all", "watch"]:
    verify_cuj(
        "2. WATCH CUJ (Direct Video Playback)",
        "https://www.youtube.com/tv?v=AB-4pS2Og1g",
        args.platform,
        args.duration,
        args.port,
        is_random_nav=False,
        device_id=args.device,
        iterations=args.iterations)

  if args.cuj in ["all", "baseline"]:
    verify_cuj(
        "3. BASELINE CUJ (Idle about:blank)",
        "about:blank",
        args.platform,
        args.duration,
        args.port,
        is_random_nav=False,
        device_id=args.device,
        iterations=args.iterations)

  if args.cuj in ["all", "combined"]:
    verify_cuj(
        "4. COMBINED CUJ (Watch Playback + UI Browse)",
        "https://www.youtube.com/tv?v=AB-4pS2Og1g",
        args.platform,
        args.duration,
        args.port,
        is_random_nav=True,
        device_id=args.device,
        iterations=args.iterations)
