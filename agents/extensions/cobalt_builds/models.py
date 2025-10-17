#!/usr/bin/env vpython3
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
"""Data models for Cobalt build and run log analysis."""

from dataclasses import dataclass, field
from typing import Any, Dict, List, Tuple


@dataclass
class LogEvent:
  """Represents a single event detected in a log."""
  line_num: int
  issue_type: str
  data: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TestBoundary:
  """Represents the start and end lines of a test."""
  start_line: int
  end_line: int


@dataclass
class Issue:
  """Represents a consolidated issue found in the log."""
  issue_type: str
  context: Tuple[int, int]
  events: List[LogEvent] = field(default_factory=list)
  test_name: str = ''
  occurrences: List[Dict[str, Any]] = field(default_factory=list)
  signature: frozenset = field(default_factory=frozenset)
  handled: bool = False