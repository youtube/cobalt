# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Evergreen Arm64 Platform Test Filters."""

from starboard.evergreen.shared import test_filters as shared_test_filters


def CreateTestFilters():
  return EvergreenArm64TestFilters()


class EvergreenArm64TestFilters(shared_test_filters.TestFilters):
  """Starboard Evergreen Arm64 Platform Test Filters."""

  def GetTestFilters(self):
    filters = super(EvergreenArm64TestFilters, self).GetTestFilters()
    return filters
