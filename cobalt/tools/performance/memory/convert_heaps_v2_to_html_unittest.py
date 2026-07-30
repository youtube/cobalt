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
                                      "name_sid": 100,
                                      "parent": 0
                                  },
                                  {
                                      "id": 2,
                                      "name_sid": 101,
                                      "parent": 1
                                  },
                                  {
                                      "id": 3,
                                      "name_sid": 102,
                                      "parent": 1
                                  },
                                  {
                                      "id": 4,
                                      "name_sid": 103,
                                      "parent": 1
                                  },
                                  {
                                      "id": 5,
                                      "name_sid": 104,
                                      "parent": 1
                                  },
                                  {
                                      "id": 6,
                                      "name_sid": 105,
                                      "parent": 1
                                  },
                                  {
                                      "id": 7,
                                      "name_sid": 106,
                                      "parent": 1
                                  },
                                  {
                                      "id": 8,
                                      "name_sid": 107,
                                      "parent": 1
                                  },
                                  {
                                      "id": 9,
                                      "name_sid": 108,
                                      "parent": 1
                                  },
                                  {
                                      "id": 10,
                                      "name_sid": 109,
                                      "parent": 1
                                  },
                              ],
                              "strings": [
                                  {
                                      "id": 100,
                                      "string": "malloc"
                                  },
                                  {
                                      "id": 101,
                                      "string": "cobalt::dom::Document::Create"
                                  },
                                  {
                                      "id": 102,
                                      "string": "blink::Element::setAttribute"
                                  },
                                  {
                                      "id": 103,
                                      "string": "v8::internal::Heap::Allocate"
                                  },
                                  {
                                      "id": 104,
                                      "string": "skia::SkBitmap::allocPixels"
                                  },
                                  {
                                      "id": 105,
                                      "string": "media::DecoderSelector::Select"
                                  },
                                  {
                                      "id":
                                          106,
                                      "string":
                                          "net::SpdySessionPool::InsertSession"
                                  },
                                  {
                                      "id": 107,
                                      "string": "base::MessagePump::Run"
                                  },
                                  {
                                      "id": 108,
                                      "string": "std::__Cr::vector::vector"
                                  },
                                  {
                                      "id": 109,
                                      "string": "pc:0xe3b1234"
                                  },
                              ],
                          },
                          "allocators": {
                              "malloc": {
                                  "nodes": [2, 3, 4, 5, 6, 7, 8, 9, 10],
                                  "sizes": [
                                      100, 200, 300, 400, 500, 600, 700, 800,
                                      900
                                  ],
                                  "counts": [1, 2, 3, 4, 5, 6, 7, 8, 9],
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

      self.assertIn("Cobalt Native C++ Heap Allocation Visualizer",
                    html_content)
      # Assert symbols for all color legend subsystem categories
      self.assertIn("cobalt::dom::Document::Create", html_content)
      self.assertIn("blink::Element::setAttribute", html_content)
      self.assertIn("v8::internal::Heap::Allocate", html_content)
      self.assertIn("skia::SkBitmap::allocPixels", html_content)
      self.assertIn("media::DecoderSelector::Select", html_content)
      self.assertIn("net::SpdySessionPool::InsertSession", html_content)
      self.assertIn("base::MessagePump::Run", html_content)
      self.assertIn("std::__Cr::vector::vector", html_content)
      self.assertIn("pc:0xe3b1234", html_content)

      # Assert UI Legend and dropdown selection
      self.assertIn("Cobalt / Starboard", html_content)
      self.assertIn("Blink Web Engine", html_content)
      self.assertIn("Standard C++ / Other", html_content)
      self.assertIn("allocSelect.value = 'all'", html_content)
      self.assertIn("absDepth <= 2", html_content)

  def test_tree_construction_attaches_abs_depth(self):
    sample_heaps = {
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
                    "string": "malloc"
                },
                {
                    "id": 11,
                    "string": "cobalt::Main"
                },
            ],
        },
        "allocators": {
            "malloc": {
                "nodes": [2],
                "sizes": [4096],
                "counts": [1],
            }
        },
    }
    tree = html_converter.build_tree_for_allocator(sample_heaps, "malloc")
    self.assertEqual(tree["name"], "malloc")
    self.assertEqual(tree["absDepth"], 1)
    self.assertTrue(len(tree["children"]) > 0)
    child = tree["children"][0]
    self.assertEqual(child["name"], "malloc")
    self.assertEqual(child["absDepth"], 2)
    self.assertTrue(len(child["children"]) > 0)
    grandchild = child["children"][0]
    self.assertEqual(grandchild["name"], "cobalt::Main")
    self.assertEqual(grandchild["absDepth"], 3)

  def test_subsystem_category_color_matching_in_html(self):
    sample_snapshots = [(
        1000000,
        {
            "allocators": {
                "malloc": {
                    "name": "malloc",
                    "value": 100,
                    "count": 1,
                    "absDepth": 1,
                    "children": [],
                }
            }
        },
    )]
    html = html_converter.generate_html_report(sample_snapshots)
    self.assertIn("function getNodeColor(node)", html)
    # Check Cobalt / Starboard
    self.assertIn("s.includes('cobalt') || s.includes('starboard')", html)
    self.assertIn("hsl(215, 100%, 34%)", html)
    # Check Blink Web Engine & WTF
    self.assertIn("s.includes('blink') || s.includes('wtf')", html)
    self.assertIn("hsl(180, 100%, 32%)", html)
    # Check V8 Engine
    self.assertIn("s.includes('v8/') || s.includes('v8::')", html)
    self.assertIn("hsl(275, 85%, 52%)", html)
    # Check Skia & GPU
    self.assertIn("s.includes('skia') || s.includes('skpath')", html)
    self.assertIn("hsl(25, 95%, 48%)", html)
    # Check Media & SbPlayer
    self.assertIn("s.includes('media/') || s.includes('media::')", html)
    self.assertIn("hsl(50, 100%, 42%)", html)
    # Check Network, Mojo, Ipcz, Spdy
    self.assertIn("s.includes('net/') || s.includes('net::')", html)
    self.assertIn("s.includes('spdy')", html)
    self.assertIn("hsl(325, 85%, 48%)", html)
    # Check Base Infrastructure & Content
    self.assertIn("s.includes('base/') || s.includes('base::')", html)
    self.assertIn("hsl(205, 75%, 58%)", html)
    # Check STL / Uncategorized Fallback
    self.assertIn("return 'hsl(0, 0%, 50%)'", html)

  def test_nonexistent_trace_returns_false(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      nonexistent = os.path.join(temp_dir, "does_not_exist.json")
      output = os.path.join(temp_dir, "out.html")
      success = html_converter.convert_trace_to_html(nonexistent, output)
      self.assertFalse(success)
      self.assertFalse(os.path.exists(output))


if __name__ == "__main__":
  unittest.main()
