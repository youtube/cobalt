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
"""Abstract interface for injecting application lifecycle transitions."""

import abc


class LifecycleController(abc.ABC):
  """Abstract interface for injecting lifecycle transitions into Cobalt."""

  @abc.abstractmethod
  def blur(self) -> None:
    """Transitions the application to blurred (loss of focus)."""

  @abc.abstractmethod
  def focus(self) -> None:
    """Transitions the application to focused / active."""

  @abc.abstractmethod
  def conceal(self) -> None:
    """Transitions the application to concealed (hidden/background)."""

  @abc.abstractmethod
  def freeze(self) -> None:
    """Transitions the application to frozen (suspended)."""

  @abc.abstractmethod
  def resume(self) -> None:
    """Transitions the application from frozen/concealed back to visible."""

  @abc.abstractmethod
  def stop(self) -> None:
    """Gracefully requests the application to terminate."""

  @abc.abstractmethod
  def low_memory(self) -> None:
    """Injects a low-memory warning event."""
