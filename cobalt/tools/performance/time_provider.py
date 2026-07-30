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
"""Provides time-related operations."""

import time
from typing import Tuple


class TimeProvider:
  """Abstracts time-related operations for testability."""

  def sleep(self, seconds: float):
    """Pauses execution for the given number of seconds."""
    time.sleep(seconds)

  def monotonic_ns(self) -> int:
    """Returns the value of a monotonic clock in nanoseconds."""
    return time.monotonic_ns()

  def strftime(self, format_string: str, t: Tuple[int, ...]) -> str:
    """Formats a time tuple to a string."""
    return time.strftime(format_string, t)

  def localtime(self) -> Tuple[int, ...]:
    """Returns a time tuple representing local time."""
    return time.localtime()
