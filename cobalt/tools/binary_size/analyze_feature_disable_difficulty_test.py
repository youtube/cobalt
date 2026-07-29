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
"""Tests for analyze_feature_disable_difficulty.py."""

import unittest
from unittest.mock import MagicMock, mock_open, patch

# Import the module to test
import cobalt.tools.binary_size.analyze_feature_disable_difficulty as analyzer


class MockArgs:
  """Simple mock class for argparse namespace."""

  def __init__(self, symbol=None, path=None, namespace=None, component=None):
    self.symbol = symbol
    self.path = path
    self.namespace = namespace
    self.component = component


class MockSymbol:
  """Mock class representing a SuperSize symbol."""

  def __init__(self, pss=0.0, source_path=None, object_path=None):
    self.pss = pss
    self.source_path = source_path
    self.object_path = object_path


class MockCommandResult:
  """Mock class for subprocess.run return value."""

  def __init__(self, returncode, stdout, stderr=""):
    self.returncode = returncode
    self.stdout = stdout
    self.stderr = stderr


class TestDeduceFeatureKeyword(unittest.TestCase):
  """Test cases for deduce_feature_keyword."""

  def test_deduce_from_symbol(self):
    args = MockArgs(symbol="WebRTC_PeerConnection")
    keyword = analyzer.deduce_feature_keyword(args)
    self.assertEqual(keyword, "webrtc_peerconnection")

  def test_deduce_from_path(self):
    args = MockArgs(path="third_party/blink/renderer/modules/peerconnection/")
    keyword = analyzer.deduce_feature_keyword(args)
    self.assertEqual(keyword, "peerconnection")

  def test_deduce_from_namespace(self):
    args = MockArgs(namespace="v8::internal::maglev")
    keyword = analyzer.deduce_feature_keyword(args)
    self.assertEqual(keyword, "maglev")

  def test_deduce_from_component(self):
    args = MockArgs(component="Blink>WebRTC>PeerConnection")
    keyword = analyzer.deduce_feature_keyword(args)
    self.assertEqual(keyword, "peerconnection")


class TestCalculateFeatureSize(unittest.TestCase):
  """Test cases for calculate_feature_size."""

  @patch("builtins.print")
  def test_calculate_size(self, mock_print):
    syms = [
        MockSymbol(pss=100.0),
        MockSymbol(pss=250.5),
        MockSymbol(pss=50.0),
    ]
    analyzer.calculate_feature_size(syms)
    mock_print.assert_called_with(
        "[Result] Total PSS of matched symbols: 400.5 bytes")


class TestGNReachabilityAndDiscovery(unittest.TestCase):
  """Test cases for GN refs discovery and reachability filtering."""

  def test_filter_reachable_targets(self):
    # Set up reachable dependencies mock
    stdout = (
        "//cobalt:cobalt\n"
        "//cobalt/dom:dom\n"
        "//third_party/blink/renderer/modules/peerconnection:peerconnection\n"
        "//unrelated/target:target\n")
    mock_runner = MagicMock(return_value=MockCommandResult(0, stdout))

    gn_targets = [
        "//cobalt/dom:dom",
        "//third_party/blink/renderer/modules/peerconnection:peerconnection",
        "//dead/target:dead",
    ]

    reachable = analyzer.filter_reachable_targets(gn_targets, "out/Default",
                                                  "//cobalt:gn_all",
                                                  mock_runner)

    # Only dom and peerconnection targets should be reachable.
    # dead target is omitted because it is not present in the desc deps output.
    self.assertIn("//cobalt/dom:dom", reachable)
    self.assertIn(
        "//third_party/blink/renderer/modules/peerconnection:peerconnection",
        reachable)
    self.assertNotIn("//dead/target:dead", reachable)

  def test_discover_feature_targets(self):

    def mock_runner(cmd):
      # gn refs query
      if "refs" in cmd:
        return MockCommandResult(
            0, "//cobalt/dom:dom\n//cobalt/browser:browser\n")
      # gn desc reachability query
      elif "desc" in cmd:
        return MockCommandResult(0, "//cobalt/dom:dom\n")
      return MockCommandResult(1, "")

    feature_files = ["cobalt/dom/window.cc"]
    targets = analyzer.discover_feature_targets(feature_files, "out/Default",
                                                "//cobalt:gn_all", mock_runner)

    # window.cc compiled by //cobalt/dom:dom, which is reachable
    self.assertEqual(targets, ["//cobalt/dom:dom"])


class TestClassifyTarget(unittest.TestCase):
  """Test cases for classify_target."""

  def test_classify_keyword_dedicated(self):
    # If keyword is a segment match, it is DEDICATED by definition
    target = (
        "//third_party/blink/renderer/modules/peerconnection:peerconnection")
    keyword = "peerconnection"
    target_type, _, _ = analyzer.classify_target(target, set(), keyword,
                                                 "out/Default", None)

    self.assertEqual(target_type, "DEDICATED")

  def test_classify_assembly_target(self):
    # If GN target has no sources (e.g. a config group), it is ASSEMBLY
    mock_runner = MagicMock(return_value=MockCommandResult(0, "\n"))
    target_type, matched, all_srcs = analyzer.classify_target(
        "//cobalt:group_target", set(), "keyword", "out/Default", mock_runner)

    self.assertEqual(target_type, "ASSEMBLY")
    self.assertEqual(matched, [])
    self.assertEqual(all_srcs, [])

  def test_classify_dedicated_by_ratio(self):
    # Target has 3 sources. 2 are compiled as feature files, and we pair 1
    # header.
    # Ratio = 3/3 = 1.0 >= 0.7 -> DEDICATED.
    sources = ("//cobalt/dom/feature.cc\n"
               "//cobalt/dom/feature.h\n"
               "//cobalt/dom/unrelated.cc\n")
    mock_runner = MagicMock(return_value=MockCommandResult(0, sources))

    # Feature files has feature.cc
    feature_files = {"cobalt/dom/feature.cc"}

    target_type, _, _ = analyzer.classify_target("//cobalt/dom:dom",
                                                 feature_files, "some_keyword",
                                                 "out/Default", mock_runner)

    # Ratio of matched (feature.cc + paired feature.h = 2) out of 3 total
    # sources is 2/3 = 66.6%
    # If ratio < 0.7, it should be classified as SHARED!
    self.assertEqual(target_type, "SHARED")

    # Now let's test with ratio >= 0.7. Let's make all files part of the
    # feature.
    feature_files_high = {"cobalt/dom/feature.cc", "cobalt/dom/unrelated.cc"}
    target_type_high, _, _ = analyzer.classify_target("//cobalt/dom:dom",
                                                      feature_files_high,
                                                      "some_keyword",
                                                      "out/Default",
                                                      mock_runner)
    self.assertEqual(target_type_high, "DEDICATED")


class TestCPPDependencyAuditing(unittest.TestCase):
  """Test cases for audit_cpp_integration_points."""

  def test_audit_includes_found(self):
    file_content = ("#include <iostream>\n"
                    '#include "cobalt/dom/window.h"\n'
                    '#include "cobalt/dom/feature.h"\n')

    feature_headers = ["cobalt/dom/feature.h"]
    files_to_scan = ["cobalt/dom/window.cc"]

    with patch("os.path.exists", return_value=True), \
         patch("builtins.open", mock_open(read_data=file_content)):
      reports = analyzer.audit_cpp_integration_points(files_to_scan,
                                                      feature_headers, False)

      self.assertEqual(len(reports), 1)
      self.assertEqual(reports[0]["file"], "cobalt/dom/window.cc")
      self.assertIn('#include "cobalt/dom/feature.h"',
                    reports[0]["referenced_headers"])

  def test_audit_includes_not_found(self):
    file_content = ("#include <iostream>\n"
                    '#include "cobalt/dom/window.h"\n')

    feature_headers = ["cobalt/dom/feature.h"]
    files_to_scan = ["cobalt/dom/window.cc"]

    with patch("os.path.exists", return_value=True), \
         patch("builtins.open", mock_open(read_data=file_content)):
      reports = analyzer.audit_cpp_integration_points(files_to_scan,
                                                      feature_headers, False)

      self.assertEqual(reports, [])


class TestDifficultyScoring(unittest.TestCase):
  """Test cases for score_target_difficulty and calculate_overall_difficulty."""

  def test_score_difficulty_low(self):
    # 1 feature file, 1 include caller
    # Complexity points = 1*1 + 1*5 = 6 pts -> LOW
    diff, action = analyzer.score_target_difficulty(1, 1)
    self.assertEqual(diff, "LOW")
    self.assertIn("Low complexity", action)

  def test_score_difficulty_medium(self):
    # 5 feature files, 5 include callers
    # Complexity points = 5*1 + 5*5 = 30 pts -> MEDIUM
    diff, action = analyzer.score_target_difficulty(5, 5)
    self.assertEqual(diff, "MEDIUM")
    self.assertIn("Moderate complexity", action)

  def test_score_difficulty_high(self):
    # 10 feature files, 10 include callers
    # Complexity points = 10*1 + 10*5 = 60 pts -> HIGH
    diff, action = analyzer.score_target_difficulty(10, 10)
    self.assertEqual(diff, "HIGH")
    self.assertIn("High complexity", action)

  def test_calculate_overall_difficulty(self):
    assessments = [
        {
            "difficulty": "LOW"
        },
        {
            "difficulty": "MEDIUM"
        },
        {
            "difficulty": "LOW"
        },
    ]
    self.assertEqual(
        analyzer.calculate_overall_difficulty(assessments), "MEDIUM")

    assessments_high = [
        {
            "difficulty": "LOW"
        },
        {
            "difficulty": "HIGH"
        },
        {
            "difficulty": "MEDIUM"
        },
    ]
    self.assertEqual(
        analyzer.calculate_overall_difficulty(assessments_high), "HIGH")


class TestSuggestRelatedFeaturePaths(unittest.TestCase):
  """Test cases for suggest_related_feature_paths."""

  def test_suggest_related_paths(self):
    # Mock symbols collection
    mock_symbols = [
        MockSymbol(source_path="cobalt/dom/feature.cc"),
        MockSymbol(source_path="cobalt/dom/feature_helper.cc"),
        MockSymbol(source_path="cobalt/browser/feature_bridge.cc"),
    ]
    size_info = MagicMock()
    size_info.symbols.WherePathMatches.return_value = mock_symbols

    # We query with current path 'cobalt/dom'
    related = analyzer.suggest_related_feature_paths(size_info, "feature",
                                                     "cobalt/dom")

    # The suggestor should return the other directory 'cobalt/browser'
    # while excluding the current query path 'cobalt/dom'
    self.assertEqual(related, ["cobalt/browser"])


if __name__ == "__main__":
  unittest.main()
