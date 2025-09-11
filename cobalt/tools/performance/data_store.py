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
"""Handles the collection of monitoring data."""

from typing import List, Dict, Any


class MonitoringDataStore:
  """Manages the storage and retrieval of monitoring data snapshots."""

  def __init__(self):
    self._data: List[Dict[str, Any]] = []

  def add_snapshot(self, snapshot: Dict[str, Any]):
    """Adds a new data snapshot to the store."""
    self._data.append(snapshot.copy())

  def get_all_data(self) -> List[Dict[str, Any]]:
    """Returns a copy of all collected monitoring data."""
    return self._data[:]

  def clear_data(self):
    """Clears all collected data from the store."""
    self._data = []
