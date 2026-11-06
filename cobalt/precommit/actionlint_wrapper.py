#!/usr/bin/env python3
#
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
"""Wrapper to run actionlint on GitHub workflows and composite actions."""

import os
import subprocess
import sys
from typing import List
import yaml

_MATRIX = {
    "config": ["devel"],
    "platform": ["linux-x64x11"],
    "shard": [0],
    "sb_api_version": [16],
    "dimension": ["default"],
}


def _lint_composite_action(path: str) -> int:
  """Transpiles a composite action into a synthetic workflow for actionlint."""
  with open(path, encoding="utf-8") as f:
    data = yaml.safe_load(f)
  if (not isinstance(data, dict) or "runs" not in data or
      not isinstance(data["runs"], dict) or "steps" not in data["runs"]):
    print(
        f"ERROR: {path} is not a valid composite action "
        "(missing 'runs.steps').",
        file=sys.stderr,
    )
    return 1
  job = {
      "runs-on": "ubuntu-latest",
      "strategy": {
          "matrix": _MATRIX
      },
      "steps": data["runs"]["steps"],
  }
  if "outputs" in data:
    job["outputs"] = {k: v["value"] for k, v in data["outputs"].items()}
  workflow = {
      "on": {
          "workflow_call": {
              "inputs": {
                  k: {
                      "type": "string"
                  } for k in data.get("inputs", {})
              }
          }
      },
      "jobs": {
          "action": job
      },
  }
  return subprocess.run(
      ["actionlint", "-stdin-filename", path, "-"],
      input=yaml.dump(workflow),
      text=True,
      check=False,
  ).returncode


def main(argv: List[str]) -> int:
  existing = [p for p in argv if os.path.exists(p)]
  workflows = [p for p in existing if not p.startswith(".github/actions/")]
  actions = [p for p in existing if p.startswith(".github/actions/")]
  rc = subprocess.call(["actionlint"] + workflows) if workflows else 0
  for path in actions:
    rc |= _lint_composite_action(path)
  if rc:
    print("ERROR: actionlint validation failed.", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
