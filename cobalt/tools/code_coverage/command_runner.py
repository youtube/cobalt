#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""An internal utility script designed to standardize the execution of
   shell commands within this suite of coverage tools"""

import subprocess
import sys


def run_command(cmd, check=True):
  """
  Executes a command, streams its output, and exits if it fails.
  """
  print(f"--- Running: {' '.join(cmd)} ---")
  try:
    # We stream the output instead of capturing it to provide real-time
    # feedback in a CI environment.
    with subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True) as process:

      for line in iter(process.stdout.readline, ''):
        sys.stdout.write(line)

      process.wait()
      if check and process.returncode != 0:
        print(
            f"\n--- Command failed with exit code {process.returncode} ---",
            file=sys.stderr)
        sys.exit(1)
  except FileNotFoundError as e:
    print(f"Error: Command not found - {e.filename}", file=sys.stderr)
    sys.exit(1)
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"An unexpected error occurred: {e}", file=sys.stderr)
    sys.exit(1)
