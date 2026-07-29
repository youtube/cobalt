#!/usr/bin/env python3
# Copyright 2026 Google LLC
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

"""RDK Flashing Script (ADB Prerequisite + USB Flashing).

PREREQUISITES:
--------------
1. Connect RDK box via USB-C (OTG/ADB).
2. Connect serial cable via Micro-USB.
3. ADB must be working and detect the device ('adb devices' shows device).
4. Burn tool (adnl_burn_pkg) and upgrade image (.img) downloaded locally.

Usage:
------
  python3 flash_rdk.py -t /path/to/adnl_burn_pkg -i /path/to/aml_upgrade_package.img
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time


def check_and_reboot_adb():
  """Checks if device is accessible via ADB and reboots it to update mode."""
  print("[INFO] Checking for connected ADB devices...")
  try:
    result = subprocess.run(
        ["adb", "devices"], capture_output=True, text=True, check=True
    )
  except FileNotFoundError:
    sys.exit("[ERROR] 'adb' command not found. Please install Android platform tools / adb.")
  except subprocess.CalledProcessError as e:
    sys.exit(f"[ERROR] Failed to run 'adb devices': {e}")

  devices = []
  for line in result.stdout.splitlines()[1:]:
    parts = line.strip().split()
    if len(parts) >= 2 and parts[1] == "device":
      devices.append(parts[0])

  if not devices:
    print("\n" + "=" * 70)
    print("[ERROR] No ADB device detected.")
    print("Prerequisite for flashing: The RDK box MUST be connected and")
    print("accessible via ADB before flashing.")
    print("\n>>> If your device is ALREADY in download/adnl mode, rerun with '-s':")
    print("    python3 flash_rdk.py -s -t <tool_path> -i <image_path>")
    print("\nOtherwise, please verify:")
    print("  1. USB-C cable is firmly connected between RDK box and host.")
    print("  2. The RDK box is powered on.")
    print("  3. 'adb devices' lists your device.")
    print("=" * 70 + "\n")
    sys.exit(1)

  target_dev = devices[0]
  print(f"[INFO] Detected ADB device: {target_dev}")
  print("[INFO] Rebooting RDK box into update/download mode via ADB...")

  try:
    subprocess.run(
        ["adb", "-s", target_dev, "shell", "reboot", "update", "-f"],
        capture_output=True,
        timeout=10,
    )
  except Exception:
    pass

  try:
    subprocess.run(
        ["adb", "-s", target_dev, "reboot", "update"],
        capture_output=True,
        timeout=10,
    )
  except Exception:
    pass

  print("[INFO] Reboot update command sent successfully via ADB.")


def flash_image(tool_path, image_file, erase=1, reboot=1):
  """Runs the adnl_burn_pkg tool with binary-safe output reading."""
  print("\n" + "=" * 70)
  print(f"[FLASHING] Running {tool_path} with image {image_file}...")
  print("=" * 70 + "\n")

  try:
    os.chmod(tool_path, 0o755)
  except OSError:
    pass

  cmd = ["sudo", tool_path, "-r", str(reboot), "-e", str(erase), "-p", image_file]
  print(f">>> Executing: {' '.join(cmd)}\n")

  log_file = "flash_log.txt"
  try:
    with (
        open(log_file, "w", encoding="utf-8", errors="replace") as log,
        subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        ) as proc,
    ):
      if proc.stdout is None:
        sys.exit("[ERROR] Failed to capture stdout from flashing tool.")

      # Read binary chunks to avoid any UTF-8 decoding crashes
      buffer = b""
      while True:
        chunk = proc.stdout.read(128)
        if not chunk:
          if proc.poll() is not None:
            break
          time.sleep(0.02)
          continue

        buffer += chunk
        while b"\n" in buffer or b"\r" in buffer:
          delimiter_idx = -1
          for i, byte_val in enumerate(buffer):
            if byte_val in (ord(b"\n"), ord(b"\r")):
              delimiter_idx = i
              break

          line_bytes = buffer[:delimiter_idx]
          buffer = buffer[delimiter_idx + 1:]

          line_str = line_bytes.decode("utf-8", errors="replace").strip()
          if line_str:
            log.write(line_str + "\n")
            log.flush()
            print(line_str, flush=True)

      # Print any remaining buffer
      if buffer:
        remaining_str = buffer.decode("utf-8", errors="replace").strip()
        if remaining_str:
          log.write(remaining_str + "\n")
          print(remaining_str, flush=True)

      proc.wait()
      if proc.returncode != 0:
        sys.exit(f"\n[ERROR] Flashing failed with exit code {proc.returncode}. See {log_file} for details.")
      else:
        print("\n[SUCCESS] Flashing completed successfully!")
  except Exception as e:
    sys.exit(f"\n[ERROR] Failed to execute flashing tool: {e}")


def main():
  parser = argparse.ArgumentParser(description="Flash RDK box via ADB + USB")
  parser.add_argument("-i", "--image", required=True, help="Path to aml_upgrade_package.img")
  parser.add_argument("-t", "--tool", "--tool-path", dest="tool", required=True, help="Path to adnl_burn_pkg binary")
  parser.add_argument("-e", "--erase", type=int, choices=[0, 1], default=1, help="Erase flash (default: 1)")
  parser.add_argument("-r", "--reboot", type=int, choices=[0, 1], default=1, help="Reboot after burn (default: 1)")
  parser.add_argument("-s", "--skip-reboot", action="store_true", help="Skip ADB reboot (if device is already in download mode)")
  args = parser.parse_args()

  image = os.path.abspath(os.path.expanduser(args.image))
  if not os.path.exists(image):
    sys.exit(f"[ERROR] Image file not found: {image}")

  tool = os.path.abspath(os.path.expanduser(args.tool))
  if not os.path.exists(tool):
    sys.exit(f"[ERROR] Flashing tool not found at: {tool}")

  print(f"[INFO] Using flashing tool: {tool}")

  # 1. Require ADB and reboot device (unless skipped)
  if not args.skip_reboot:
    check_and_reboot_adb()
    time.sleep(2)

  # 2. Flash image over USB
  flash_image(tool, image, erase=args.erase, reboot=args.reboot)


if __name__ == "__main__":
  main()
