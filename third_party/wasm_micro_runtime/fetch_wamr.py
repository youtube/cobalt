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
"""Fetches WAMR (WebAssembly Micro Runtime) from upstream into src/.

BUILD.gn runs this at gn time, so a fresh checkout builds without a manual
step. src/ is git-ignored. Needs network access the first time only; once src/
is at the pinned commit, this does nothing.
"""

import os
import subprocess
import sys

REPO = "https://github.com/bytecodealliance/wasm-micro-runtime.git"
# Upstream commit, closest release WAMR-2.4.4. Same as google3's
# //third_party/wasm_micro_runtime.
COMMIT = "8c18e3f68b16c4bcaf05996b2636f6ed2b4cf629"

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")


def git(*args):
  return subprocess.run(["git", "-C", SRC] + list(args),
                        check=True,
                        capture_output=True,
                        text=True).stdout.strip()


def main():
  if os.path.isdir(os.path.join(SRC, ".git")):
    try:
      if git("rev-parse", "HEAD") == COMMIT:
        return 0
    except subprocess.CalledProcessError:
      pass
  else:
    os.makedirs(SRC, exist_ok=True)
    git("init", "-q")
    git("remote", "add", "origin", REPO)
  try:
    git("fetch", "--depth", "1", "origin", COMMIT)
    git("checkout", "-q", "--detach", "FETCH_HEAD")
  except subprocess.CalledProcessError as e:
    sys.stderr.write(f"fetch_wamr.py: failed to fetch WAMR {COMMIT} from "
                     f"{REPO}:\n{e.stderr}")
    return 1
  sys.stderr.write(f"fetch_wamr.py: fetched WAMR {COMMIT} into {SRC}\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
