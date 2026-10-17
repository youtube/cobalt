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
"""Significance Engine Simulator and Statistical Power Analyst for Cobalt."""
# pylint: disable=wrong-import-position
import argparse
import logging
import os
import random
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from stats_utils import calculate_stats, permutation_test_p_value


def run_simulation(case_name, baseline, experiment, sig_threshold=0.05):
  """Simulates a single significance testing case."""
  b_mean, _, b_std = calculate_stats(baseline)
  e_mean, _, e_std = calculate_stats(experiment)
  diff = b_mean - e_mean

  p_val = permutation_test_p_value(baseline, experiment)
  is_sig = p_val < sig_threshold

  logging.info("🔬 SIMULATION CASE: %s", case_name)
  logging.info("%s", "-" * 55)
  b_fmt = [f"{x:.2f}" for x in baseline]
  e_fmt = [f"{x:.2f}" for x in experiment]
  logging.info("   • Baseline:   %s (Mean: %.2f, StdDev: %.2f)", b_fmt, b_mean,
               b_std)
  logging.info("   • Experiment: %s (Mean: %.2f, StdDev: %.2f)", e_fmt, e_mean,
               e_std)
  logging.info("   • Difference: %.2f MB (Reduction: %.1f%%)", diff,
               (diff / b_mean) * 100.0 if b_mean > 0 else 0.0)
  logging.info("   • p-value:    %.4f", p_val)
  conclusion = "STATISTICALLY SIGNIFICANT" if is_sig else "NOT SIGNIFICANT"
  logging.info("   📢 CONCLUSION: %s", conclusion)
  logging.info("%s\n", "=" * 55)
  return is_sig


def generate_synthetic_normal(mean, std_dev, n):
  """Generates safe normally distributed numbers using standard library."""
  data = []
  for _ in range(n):
    val = random.gauss(mean, std_dev)
    data.append(max(1.0, val))  # Clamp to positive values
  return data


def run_aa_calibration(n_runs=5,
                       iterations=100,
                       mean=270.0,
                       std_dev=5.0,
                       alpha=0.05):
  """Runs A/A calibration to ensure false positive rate matches Alpha."""
  logging.info("⚡ RUNNING A/A CALIBRATION (Iterations=%s, N=%s)", iterations,
               n_runs)
  logging.info("   Parameters: Normal (Mean=%.1f MB, StdDev=%.1f MB)", mean,
               std_dev)
  logging.info("   Evaluating false positive rate under identical configs...")

  false_positives = 0
  for _ in range(iterations):
    g1 = generate_synthetic_normal(mean, std_dev, n_runs)
    g2 = generate_synthetic_normal(mean, std_dev, n_runs)
    p_val = permutation_test_p_value(g1, g2, permutations=1000)
    if p_val < alpha:
      false_positives += 1

  fp_rate = (false_positives / iterations) * 100.0
  logging.info("%s", "-" * 55)
  logging.info("   • Total False Positives (p < %s): %s out of %s", alpha,
               false_positives, iterations)
  logging.info(
      "   • Empirical False Positive Rate:      %.1f%% "
      "(Expected: ~%.1f%%)", fp_rate, alpha * 100.0)
  if abs(fp_rate - alpha * 100.0) <= 3.0:
    logging.info("   🟢 CALIBRATION SUCCESSFUL: perfectly calibrated! 🟢")
  else:
    logging.info(
        "   ⚠️ CALIBRATION WARNING: empirical rate deviates from Alpha.")
  logging.info("%s\n", "=" * 55)


def main():
  logging.basicConfig(level=logging.INFO, format="%(message)s")
  parser = argparse.ArgumentParser(
      description="Significance Engine Simulator & Power Analyst")
  parser.add_argument(
      "--runs", type=int, default=5, help="Number of runs per group")
  parser.add_argument(
      "--iterations",
      type=int,
      default=100,
      help="Number of simulator iterations")
  args = parser.parse_args()

  logging.info("%s", "=" * 55)
  logging.info("📊 RUNNING HYPOTHESIS TESTING SIMULATOR")
  logging.info("%s\n", "=" * 55)

  # Case 1: Small memory win with moderate variance (Expected: NOT SIGNIFICANT)
  b1 = [275.5, 278.1, 272.9, 276.4, 274.8]
  e1 = [274.2, 276.8, 271.5, 275.1, 273.4]
  run_simulation("Case 1 (Small Reduction, N=5)", b1, e1)

  # Case 2: Massive memory win (Expected: STATISTICALLY SIGNIFICANT)
  b2 = [275.5, 278.1, 272.9, 276.4, 274.8]
  e2 = [250.1, 254.8, 248.6, 252.3, 251.0]
  run_simulation("Case 2 (Massive Reduction, N=5)", b2, e2)

  # Run A/A calibration
  run_aa_calibration(n_runs=args.runs, iterations=args.iterations)


if __name__ == "__main__":
  main()
