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
"""Abstract interface for platform display and windowing environments."""

import abc


class PlatformDisplay(abc.ABC):
  """Abstract interface for platform display and windowing environments."""

  @abc.abstractmethod
  def is_working(self) -> bool:
    """Returns whether a functional display environment is available."""

  @abc.abstractmethod
  def ensure_display(self) -> None:
    """Ensures a working display is present; re-execs or configures."""
