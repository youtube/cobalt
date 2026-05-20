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

import argparse
import math
import random


def calculate_stats(data):
  """Calculates mean and standard deviation of a numeric array."""
  n = len(data)
  if n == 0:
    return 0.0, 0.0
  mean = sum(data) / n
  variance = sum((x - mean)**2 for x in data) / max(1, n - 1)
  std_dev = math.sqrt(variance)
  return mean, std_dev


def permutation_test_p_value(group1, group2, permutations=10000):
  """Calculates p-value via permutation test."""
  n1 = len(group1)
  n2 = len(group2)
  if n1 == 0 or n2 == 0:
    return 1.0

  mean1 = sum(group1) / n1
  mean2 = sum(group2) / n2
  observed_diff = abs(mean1 - mean2)

  pooled = group1 + group2
  count = 0
  for _ in range(permutations):
    random.shuffle(pooled)
    perm_g1 = pooled[:n1]
    perm_g2 = pooled[n1:]
    perm_diff = abs((sum(perm_g1) / n1) - (sum(perm_g2) / n2))
    if perm_diff >= observed_diff:
      count += 1
  return count / permutations


def run_simulation(case_name, baseline, experiment, sig_threshold=0.05):
  """Simulates a single significance testing case."""
  b_mean, b_std = calculate_stats(baseline)
  e_mean, e_std = calculate_stats(experiment)
  diff = b_mean - e_mean

  p_val = permutation_test_p_value(baseline, experiment)
  is_sig = p_val < sig_threshold

  print(f"🔬 SIMULATION CASE: {case_name}")
  print("-" * 55)
  b_fmt = [f"{x:.2f}" for x in baseline]
  e_fmt = [f"{x:.2f}" for x in experiment]
  print(f"   • Baseline:   {b_fmt} (Mean: {b_mean:.2f}, StdDev: {b_std:.2f})")
  print(f"   • Experiment: {e_fmt} (Mean: {e_mean:.2f}, StdDev: {e_std:.2f})")
  print(f"   • Difference: {diff:.2f} MB "
        f"(Reduction: {(diff / b_mean) * 100.0:.1f}%)")
  print(f"   • p-value:    {p_val:.4f}")
  conclusion = "STATISTICALLY SIGNIFICANT" if is_sig else "NOT SIGNIFICANT"
  print(f"   📢 CONCLUSION: {conclusion}")
  print("=" * 55 + "\n")
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
  print(f"⚡ RUNNING A/A CALIBRATION (Iterations={iterations}, N={n_runs})")
  print(f"   Parameters: Normal (Mean={mean} MB, StdDev={std_dev} MB)")
  print("   Evaluating false positive rate under identical configs...")

  false_positives = 0
  for _ in range(iterations):
    g1 = generate_synthetic_normal(mean, std_dev, n_runs)
    g2 = generate_synthetic_normal(mean, std_dev, n_runs)
    p_val = permutation_test_p_value(g1, g2, permutations=1000)
    if p_val < alpha:
      false_positives += 1

  fp_rate = (false_positives / iterations) * 100.0
  print("-" * 55)
  print(f"   • Total False Positives (p < {alpha}): "
        f"{false_positives} out of {iterations}")
  print(f"   • Empirical False Positive Rate:      {fp_rate:.1f}% "
        f"(Expected: ~{alpha * 100.0:.1f}%)")
  if abs(fp_rate - alpha * 100.0) <= 3.0:
    print("   🟢 CALIBRATION SUCCESSFUL: perfectly calibrated! 🟢")
  else:
    print("   ⚠️ CALIBRATION WARNING: empirical rate deviates from Alpha.")
  print("=" * 55 + "\n")


def main():
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

  print("=" * 55)
  print("📊 RUNNING HYPOTHESIS TESTING SIMULATOR")
  print("=" * 55 + "\n")

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
