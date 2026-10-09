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
"""Unit tests for analyze_memory_evaluator_frequency.py."""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import analyze_memory_evaluator_frequency as amef


class AnalyzeMemoryEvaluatorFrequencyTest(unittest.TestCase):

  def test_parse_timestamp_syslog(self):
    line = "Sep 17 07:20:43 AmlogicFirebolt WPEFramework[32713]: something"
    dt, ts_str = amef.parse_timestamp(line)
    self.assertIsNotNone(dt)
    self.assertEqual(ts_str, "Sep 17 07:20:43")
    self.assertEqual(dt.month, 9)
    self.assertEqual(dt.day, 17)

  def test_parse_timestamp_cobalt(self):
    line = ("[32713:CrBrowserMain/32719:0917/032043.354663:INFO:evaluator.cc]"
            " something")
    dt, ts_str = amef.parse_timestamp(line)
    self.assertIsNotNone(dt)
    self.assertEqual(ts_str, "0917 03:20:43")
    self.assertEqual(dt.month, 9)
    self.assertEqual(dt.day, 17)

  def test_parse_timestamp_dmesg(self):
    line = "[  123.456789] Out of memory"
    dt, ts_str = amef.parse_timestamp(line)
    self.assertIsNotNone(dt)
    self.assertEqual(ts_str, "123.46s")

  def test_parse_evaluator_logs_and_analysis(self):
    sample_log = """
Sep 17 07:19:53 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator initialized: baseline_budget=210 MB, moderate_threshold=85%, critical_threshold=95%, poll_interval=5.0 s, cooldown=15.0 s
Sep 17 07:20:00 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=150 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=71%, level=NONE
Sep 17 07:20:05 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=185 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=88%, level=MODERATE
Sep 17 07:20:05 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: [DISPATCH] MODERATE reason=transition
Sep 17 07:20:10 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=186 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=88%, level=MODERATE
Sep 17 07:20:20 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=187 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=89%, level=MODERATE
Sep 17 07:20:20 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: [DISPATCH] MODERATE reason=cooldown_expired
Sep 17 07:20:25 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=205 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=97%, level=CRITICAL
Sep 17 07:20:25 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: [DISPATCH] CRITICAL reason=transition
Sep 17 07:20:30 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=206 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=98%, level=CRITICAL
Sep 17 07:20:30 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: [DISPATCH] CRITICAL reason=cooldown_expired
Sep 17 07:20:35 AmlogicFirebolt WPEFramework[32713]: CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=190 MB, effective_budget=210 MB (base=210 MB, media=0 MB), usage=90%, level=MODERATE
"""
    data = amef.parse_evaluator_logs(sample_log)
    self.assertIsNotNone(data["init_config"])
    self.assertEqual(data["init_config"]["baseline_budget_mb"], 210)
    self.assertEqual(data["init_config"]["moderate_threshold_pct"], 85.0)
    self.assertEqual(data["init_config"]["critical_threshold_pct"], 95.0)
    self.assertEqual(data["init_config"]["poll_interval_s"], 5.0)
    self.assertEqual(data["init_config"]["cooldown_s"], 15.0)

    self.assertEqual(len(data["evaluations"]), 7)
    self.assertEqual(len(data["dispatches"]), 4)

    analysis = amef.analyze_frequency_and_cadence(data)

    # 1. Transitions verification
    self.assertEqual(len(analysis["transitions"]), 3)
    self.assertEqual(analysis["transition_counts"]["NONE -> MODERATE"], 1)
    self.assertEqual(analysis["transition_counts"]["MODERATE -> CRITICAL"], 1)
    self.assertEqual(analysis["transition_counts"]["CRITICAL -> MODERATE"], 1)

    # 2. Dwell times & duty cycle verification
    self.assertGreater(analysis["dwell_seconds"]["MODERATE"], 0.0)
    self.assertGreater(analysis["dwell_seconds"]["CRITICAL"], 0.0)
    self.assertGreater(analysis["dwell_seconds"]["NONE"], 0.0)
    self.assertIsNotNone(analysis["avg_dwell_per_episode"]["MODERATE"])

    # 3. Dispatches verification
    self.assertEqual(analysis["critical_dispatches"]["count"], 2)
    self.assertEqual(analysis["critical_dispatches"]["min_s"], 5.0)
    self.assertEqual(analysis["moderate_dispatches"]["count"], 2)
    self.assertEqual(analysis["moderate_dispatches"]["min_s"], 15.0)

    # 4. Stress checks
    self.assertEqual(analysis["flapping_count"], 0)
    self.assertEqual(analysis["moderate_cooldown_violations"], 0)
    self.assertIn("stress_verdict", analysis)


if __name__ == "__main__":
  unittest.main()
