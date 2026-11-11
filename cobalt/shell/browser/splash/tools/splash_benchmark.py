#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Splash screen startup benchmark for Cobalt on Android.

Cold-starts Cobalt N times per arm, round-robin across the arms, and reads
the startup timeline that SplashScreenMetrics logs with
--vmodule=splash_screen_metrics=1:

  NativeSplash: Cobalt.Startup.Timeline.<Event>.<Mode>: <ms>ms

all relative to the app's start (CobaltMainDelegate's construction). The main
metric is MainReady: when the web app calls h5vcc.system.hideSplashScreen().
For the native arm, also records whether the Wasm selector ran (by_wasm).

Arms:
  image         splash.html with its image (the default)
  html_video    splash.html with its video (ForceVideoSplashScreen)
  native_wamr   no splash WebContents; WAMR picks the video, SbPlayer plays
                it (--native-splash-player)

Usage:
  splash_benchmark.py --apk out/android-arm_devel/apks/Cobalt.apk --runs 30
  splash_benchmark.py --report results.csv
"""

import argparse
import csv
import math
import os
import random
import re
import statistics
import subprocess
import sys
import time

PACKAGE = "dev.cobalt.coat"
ACTIVITY = f"{PACKAGE}/dev.cobalt.app.MainActivity"
COMMON_ARGS = ["--vmodule=splash_screen_metrics=1"]
ARMS = {
    "image": [],
    "html_video": ["--enable-features=ForceVideoSplashScreen"],
    "native_wamr": ["--native-splash-player"],
}
TIMELINE_RE = re.compile(
    r"NativeSplash: Cobalt\.Startup\.Timeline\.(\w+)\.(\w+): (\d+)ms")
SELECTION_RE = re.compile(
    r"Splash selection: key='([^']*)'.*by_wasm=(\d).*selector_error='([^']*)'"
    r".*read_selector=([\d.]+) s, run_selector=([\d.]+) s")
EVENTS = ["MainNavigationStart", "MainCommit", "MainLoaded", "MainVisible",
          "MainReady", "SplashStart", "SplashContentShown"]
FIELDS = ["arm", "run", "time", "mode"] + EVENTS + [
    "key", "by_wasm", "selector_error", "read_selector_ms", "run_selector_ms",
    "crash"]


class Device:

  def __init__(self, serial):
    self.serial = serial

  def adb(self, *args, timeout=120):
    cmd = ["adb"] + (["-s", self.serial] if self.serial else []) + list(args)
    return subprocess.run(cmd, check=True, capture_output=True, text=True,
                          timeout=timeout).stdout

  def run_once(self, arm, timeout_s=40):
    self.adb("shell", "am", "force-stop", PACKAGE)
    time.sleep(3)
    self.adb("logcat", "-c")
    args = ",".join(COMMON_ARGS + ARMS[arm])
    self.adb("shell", "am", "start", "-n", ACTIVITY, "--esa",
             "commandLineArgs", args)
    deadline = time.time() + timeout_s
    result = {"arm": arm, "time": time.strftime("%H:%M:%S")}
    while time.time() < deadline:
      time.sleep(1)
      log = self.adb("logcat", "-d", "-v", "time")
      for m in TIMELINE_RE.finditer(log):
        event, mode, ms = m.groups()
        result[event] = int(ms)
        result["mode"] = mode
      m = SELECTION_RE.search(log)
      if m:
        result["key"], result["by_wasm"] = m.group(1), int(m.group(2))
        result["selector_error"] = m.group(3)
        result["read_selector_ms"] = round(float(m.group(4)) * 1000, 1)
        result["run_selector_ms"] = round(float(m.group(5)) * 1000, 1)
      if "FATAL" in log and PACKAGE in log:
        result["crash"] = 1
      if "MainReady" in result:
        break
    self.adb("shell", "am", "force-stop", PACKAGE)
    return result


def benchmark(args):
  device = Device(args.device)
  arms = args.arms.split(",")
  if args.apk:
    print(f"installing {args.apk}", file=sys.stderr)
    device.adb("install", "-r", "-d", args.apk, timeout=600)
    device.run_once(arms[0])  # The first launch after an install is slower.
  new = not os.path.exists(args.out)
  with open(args.out, "a", newline="") as f:
    w = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore")
    if new:
      w.writeheader()
    for run in range(args.runs):
      order = arms[:]
      random.shuffle(order)
      for arm in order:
        r = device.run_once(arm)
        r["run"] = run
        w.writerow(r)
        f.flush()
        print(f"{arm:12s} run {run}: MainReady={r.get('MainReady')} "
              f"mode={r.get('mode')} by_wasm={r.get('by_wasm')} "
              f"err={r.get('selector_error', '')}", file=sys.stderr)


def mann_whitney_p(a, b):
  """Two-sided Mann-Whitney U, normal approximation with tie correction."""
  n1, n2 = len(a), len(b)
  allv = sorted([(v, 0) for v in a] + [(v, 1) for v in b])
  ranks = [0.0] * len(allv)
  i = 0
  while i < len(allv):
    j = i
    while j + 1 < len(allv) and allv[j + 1][0] == allv[i][0]:
      j += 1
    for k in range(i, j + 1):
      ranks[k] = (i + j) / 2 + 1
    i = j + 1
  r1 = sum(r for r, (_, g) in zip(ranks, allv) if g == 0)
  u1 = r1 - n1 * (n1 + 1) / 2
  n = n1 + n2
  ties = {}
  for v, _ in allv:
    ties[v] = ties.get(v, 0) + 1
  tie_term = sum(t**3 - t for t in ties.values())
  sigma = math.sqrt(n1 * n2 / 12 * ((n + 1) - tie_term / (n * (n - 1))))
  if sigma == 0:
    return 1.0
  return math.erfc(abs(u1 - n1 * n2 / 2) / sigma / math.sqrt(2))


def bootstrap_diff(a, b, iters=5000):
  rng = random.Random(0)
  diffs = []
  for _ in range(iters):
    sa = [rng.choice(a) for _ in a]
    sb = [rng.choice(b) for _ in b]
    diffs.append(statistics.median(sa) - statistics.median(sb))
  diffs.sort()
  return diffs[int(0.025 * iters)], diffs[int(0.975 * iters)]


def report(args):
  rows = list(csv.DictReader(open(args.out)))
  arms = list(dict.fromkeys(r["arm"] for r in rows))
  print(f"n per arm: " +
        ", ".join(f"{a}={sum(1 for r in rows if r['arm'] == a)}" for a in arms))
  for metric in ["MainReady", "MainCommit", "MainLoaded", "MainVisible"]:
    data = {}
    for a in arms:
      v = sorted(int(r[metric]) for r in rows if r["arm"] == a and r[metric])
      if v:
        data[a] = v
    if not data:
      continue
    print(f"\n### {metric} (ms from app start)")
    print("| arm | n | p50 | p90 | min | max | sd |")
    print("|---|---:|---:|---:|---:|---:|---:|")
    for a, v in data.items():
      p90 = v[min(len(v) - 1, int(0.9 * len(v)))]
      sd = statistics.pstdev(v) if len(v) > 1 else 0
      print(f"| {a} | {len(v)} | {statistics.median(v):.0f} | {p90} | {v[0]} "
            f"| {v[-1]} | {sd:.0f} |")
    base = args.baseline if args.baseline in data else next(iter(data))
    if len(data) > 1:
      print(f"\nvs. `{base}`: median difference [bootstrap 95% CI], "
            "Mann-Whitney p")
      for a, v in data.items():
        if a == base:
          continue
        lo, hi = bootstrap_diff(v, data[base])
        d = statistics.median(v) - statistics.median(data[base])
        print(f"- `{a}`: {d:+.0f} ms [{lo:+.0f}, {hi:+.0f}], "
              f"p={mann_whitney_p(v, data[base]):.3f}")
  print("\n### selector")
  for a in arms:
    sel = [r for r in rows if r["arm"] == a and r["by_wasm"]]
    if sel:
      ran = sum(1 for r in sel if r["by_wasm"] == "1")
      rs = [float(r["read_selector_ms"]) for r in sel]
      ru = [float(r["run_selector_ms"]) for r in sel]
      errs = sorted({r["selector_error"] for r in sel if r["selector_error"]})
      print(f"- `{a}`: by_wasm {ran}/{len(sel)}, read_selector p50 "
            f"{statistics.median(rs):.0f} ms, run_selector p50 "
            f"{statistics.median(ru):.1f} ms, errors={errs}")
  crashes = [r for r in rows if r["crash"]]
  if crashes:
    print(f"\nCRASHES: {[(r['arm'], r['run']) for r in crashes]}")


def main():
  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--device", default=os.environ.get("ANDROID_SERIAL"))
  ap.add_argument("--apk", help="install this APK first")
  ap.add_argument("--runs", type=int, default=5)
  ap.add_argument("--arms", default=",".join(ARMS))
  ap.add_argument("--out", default="results.csv")
  ap.add_argument("--report", action="store_true")
  ap.add_argument("--baseline", default="image")
  args = ap.parse_args()
  if args.report:
    report(args)
  else:
    benchmark(args)


if __name__ == "__main__":
  main()
