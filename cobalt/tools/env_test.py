#!/usr/bin/env python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
"""Tests out _env."""

import sys
import unittest


class EnvironmentTest(unittest.TestCase):

  def testSysPath(self):
    import _env  # pylint: disable=unused-variable,g-import-not-at-top
    import cobalt.tools.paths  # pylint: disable=g-import-not-at-top
    self.assertTrue(cobalt.tools.paths.COBALT_ROOT)

  def testNoDupes(self):
    import _env  # pylint: disable=unused-variable,g-import-not-at-top
    visited = set()
    deduped = [x for x in sys.path if not (x in visited or visited.add(x))]
    self.assertItemsEqual(sys.path, deduped)


if __name__ == '__main__':
  unittest.main()
