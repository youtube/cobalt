#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Generate the part of the _book.yaml TOC that lists Starboard modules."""

import os
import sys
import pathlib

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
_DEVSITE_ROOT = os.path.relpath(os.path.join(_REPO_ROOT, "cobalt", "site"))


def get_modules(path: str) -> list[str]:
  return sorted([
      os.path.join(path, name)
      for name in os.listdir(path)
      if os.path.isfile(os.path.join(path, name)) and name[-3:] == ".md"
  ])


def get_old_sb_versions(path: str) -> list[str]:
  return sorted([
      os.path.join(path, name)
      for name in os.listdir(path)
      if os.path.isdir(os.path.join(path, name))
  ])[:-1]  # Skip the latest version.


def print_header(title: str, indent: int):
  print("  " * indent + f"  - title: {title}")
  print("  " * indent + "    section:")


def print_modules(modules: list[str], indent: int):
  for module_path in modules:
    rel_path = os.path.relpath(module_path, _DEVSITE_ROOT)
    print("  " * indent + f"    - title: {pathlib.Path(module_path).stem}.h")
    print("  " * indent + f"      path: /youtube/cobalt/{rel_path}")


def main() -> None:
  if len(sys.argv) > 2:
    print("Usage: generate_sb_reference.py <path>")
    sys.exit(1)

  path = sys.argv[1]

  print_header("Starboard Modules", 0)
  # Modules in current Starboard version.
  print_modules(get_modules(path), 0)

  # Modules in old SB versions. Should be indented one step.
  for sb_version_path in get_old_sb_versions(path):
    print_header(f"Starboard {os.path.basename(sb_version_path)}", 1)
    print_modules(get_modules(sb_version_path), 1)


if __name__ == "__main__":
  main()
