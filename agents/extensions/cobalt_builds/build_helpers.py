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
"""Shared helper functions for Cobalt build scripts."""

from dataclasses import dataclass, asdict

# --- Data Models ---

@dataclass
class TaskStatus:
  status: str
  output_log: str
  pid: int | None = None

  def model_dump(self) -> dict:
    return asdict(self)


# --- Platform and Variant Definitions ---
PLATFORM_ALIASES = {
    'non-hermetic': 'linux-x64x11',
    'modular': 'linux-x64x11-modular',
    'linux-x64-modular': 'linux-x64x11-modular',
    'evergreen': 'evergreen-x64',
    'android': 'android-arm',
}
VALID_PLATFORMS = tuple(PLATFORM_ALIASES.keys()) + tuple(
    PLATFORM_ALIASES.values())


def get_platform(platform_or_alias: str) -> str:
  """Resolves a platform alias to its full name."""
  return PLATFORM_ALIASES.get(platform_or_alias, platform_or_alias)


def get_out_dir(platform: str, variant: str) -> str:
  """Constructs the output directory path for a given platform and variant."""
  return f'out/{get_platform(platform)}_{variant}'
