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
"""
Generate the part of the _book.yaml Table of Contents for Starboard modules.
"""

import os
import sys
import pathlib
import typing

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
_DEVSITE_ROOT = os.path.relpath(os.path.join(_REPO_ROOT, "cobalt", "site"))


def _get_modules(path: str) -> typing.List[str]:
  """
  Get a list of all generated Starboard module markdown files in the specified
  folder.
  """
  sb_modules = [
      os.path.join(path, name)
      for name in os.listdir(path)
      if os.path.isfile(os.path.join(path, name)) and name[-3:] == ".md"
  ]
  return sorted(sb_modules)


def _get_sb_versions(module_reference_path: str) -> typing.List[str]:
  """
  Get the Starboard folders in the module reference path.
  """
  sb_versions = [
      os.path.join(module_reference_path, name)
      for name in os.listdir(module_reference_path)
      if os.path.isdir(os.path.join(module_reference_path, name))
  ]
  return sorted(sb_versions)


def _print_header(title: str):
  print(f"  - title: {title}")
  print("    section:")


def _print_modules(modules: typing.List[str]):
  for module_path in modules:
    rel_path = os.path.relpath(module_path, _DEVSITE_ROOT)
    print(f"    - title: {pathlib.Path(module_path).stem}.h")
    print(f"      path: /youtube/cobalt/{rel_path}")


def generate_toc() -> None:
  """Generate the Starboard module Table of Contents."""
  if len(sys.argv) > 2:
    print(f"Usage: {sys.argv[0]} <path>")
    sys.exit(1)

  path = sys.argv[1]

  # Print a header and link to individual module reference docs.
  for sb_version_path in _get_sb_versions(path):
    _print_header(f"Starboard {os.path.basename(sb_version_path)}")
    _print_modules(_get_modules(sb_version_path))


if __name__ == "__main__":
  generate_toc()
