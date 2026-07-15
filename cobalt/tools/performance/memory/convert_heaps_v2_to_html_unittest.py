#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Unit tests for convert_heaps_v2_to_html.py."""

import json
import os
import sys
import tempfile
import unittest

script_dir = os.path.dirname(os.path.realpath(__file__))
if script_dir not in sys.path:
  sys.path.insert(0, script_dir)

# pylint: disable=wrong-import-position
import convert_heaps_v2_to_html as html_converter


class ConvertHeapsV2ToHtmlTest(unittest.TestCase):

  def test_convert_valid_trace_to_html(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      sample_trace = {
          "traceEvents": [{
              "name": "periodic_interval",
              "ts": 1000000,
              "args": {
                  "dumps": {
                      "heaps_v2": {
                          "maps": {
                              "nodes": [
                                  {
                                      "id": 1,
                                      "name_sid": 10,
                                      "parent": 0
                                  },
                                  {
                                      "id": 2,
                                      "name_sid": 11,
                                      "parent": 1
                                  },
                              ],
                              "strings": [
                                  {
                                      "id": 10,
                                      "string": "base::TestFunction"
                                  },
                                  {
                                      "id": 11,
                                      "string": "malloc"
                                  },
                              ],
                          },
                          "allocators": {
                              "malloc": {
                                  "nodes": [2],
                                  "sizes": [1048576],
                                  "counts": [10],
                              }
                          },
                      }
                  }
              },
          }]
      }

      input_path = os.path.join(temp_dir, "trace.json")
      output_path = os.path.join(temp_dir, "flamegraph.html")

      with open(input_path, "w", encoding="utf-8") as f:
        json.dump(sample_trace, f)

      success = html_converter.convert_trace_to_html(input_path, output_path)
      self.assertTrue(success)
      self.assertTrue(os.path.exists(output_path))

      with open(output_path, "r", encoding="utf-8") as f:
        html_content = f.read()

      self.assertIn("Cobalt Heap Memory Flamegraph", html_content)
      self.assertIn("base::TestFunction", html_content)
      self.assertIn("1048576", html_content)

  def test_nonexistent_trace_returns_false(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      nonexistent = os.path.join(temp_dir, "does_not_exist.json")
      output = os.path.join(temp_dir, "out.html")
      success = html_converter.convert_trace_to_html(nonexistent, output)
      self.assertFalse(success)
      self.assertFalse(os.path.exists(output))


if __name__ == "__main__":
  unittest.main()
