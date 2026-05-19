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
"""Synthetic memory profiling significance simulation and calibration suite."""

import argparse
import math
import random
import sys


def calculate_stats(data):
  """Calculates sample mean and sample standard deviation."""
  n = len(data)
  if n == 0:
    return 0.0, 0.0
  mean = sum(data) / n
  variance = sum((x - mean)**2 for x in data) / max(1, n - 1)
  std_dev = math.sqrt(variance)
  return mean, std_dev


def permutation_test_p_value(group1, group2, permutations=10000):
  """Performs non-parametric bootstrap permutation test to calculate p-value."""
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
  """Runs a synthetic significance comparison case and prints the report."""
  b_mean, b_std = calculate_stats(baseline)
  e_mean, e_std = calculate_stats(experiment)
  diff = b_mean - e_mean

  p_val = permutation_test_p_value(baseline, experiment)
  is_sig = p_val < sig_threshold

  print(f"🔬 SIMULATION CASE: {case_name}")
  print("-" * 55)
  b_str = ", ".join(f"{x:.2f}" for x in baseline)
  e_str = ", ".join(f"{x:.2f}" for x in experiment)
  print(f"   • Baseline:   [{b_str}] (Mean: {b_mean:.2f}, StdDev: {b_std:.2f})")
  print(f"   • Experiment: [{e_str}] (Mean: {e_mean:.2f}, StdDev: {e_std:.2f})")
  print(f"   • Difference: {diff:.2f} MB "
        f"(Reduction: {(diff / b_mean) * 100.0:.1f}%)")
  print(f"   • p-value:    {p_val:.4f}")
  sig_str = "🟢 STATISTICALLY SIGNIFICANT" if is_sig else "🔴 NOT SIGNIFICANT"
  print(f"   📢 CONCLUSION: {sig_str}")
  print("=" * 55 + "\n")
  return is_sig


def generate_synthetic_normal(mean, std_dev, n):
  """Generates positive normally distributed random float samples."""
  data = []
  for _ in range(n):
    u1 = random.random()
    u2 = random.random()
    z0 = math.sqrt(-2.0 * math.log(u1)) * math.cos(2.0 * math.pi * u2)
    val = mean + z0 * std_dev
    data.append(max(1.0, val))  # Clamp to positive values
  return data


def run_aa_calibration(n_runs=5,
                       iterations=100,
                       mean=270.0,
                       std_dev=5.0,
                       alpha=0.05):
  """Calibrates significance metrics via synthetic A/A iterations."""
  print(f"⚡ RUNNING A/A HYPOTHESIS CALIBRATION "
        f"(Iterations={iterations}, N={n_runs} per group)")
  print(f"   Parameters: Normal distribution "
        f"(Mean={mean} MB, StdDev={std_dev} MB)")
  print("   Evaluating false positive rate under identical configurations...")

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
  print(f"   • Empirical False Positive Rate:      "
        f"{fp_rate:.1f}% (Expected: ~{alpha * 100.0:.1f}%)")
  if abs(fp_rate - alpha * 100.0) <= 3.0:
    print("   🟢 CALIBRATION SUCCESSFUL: "
          "Statistical engine is perfectly calibrated! 🟢")
  else:
    print("   ⚠️ CALIBRATION WARNING: "
          "Empirical false positive rate deviates from theoretical Alpha.")
  print("=" * 55 + "\n")


def main():
  parser = argparse.ArgumentParser(
      description="Significance Engine Simulator & Power Analyst")
  parser.add_argument(
      "--aa-test", action="store_true", help="Run A/A test calibration suite")
  args = parser.parse_args()

  if args.aa_test:
    run_aa_calibration()
    sys.exit(0)

  print("=" * 55)
  print("📊 RUNNING SYNTHETIC VERIFICATION CASE SUITE")
  print("=" * 55 + "\n")

  # Case 1: True Alternative Hypothesis (Large Improvement with low noise)
  run_simulation(
      "Case 1: Large Clamp Improvement with Low Process Noise",
      baseline=[270.5, 273.1, 269.2, 271.8, 272.0],  # Mean = 271.3, Std = 1.5
      experiment=[240.2, 243.1, 239.5, 241.8,
                  242.0]  # Mean = 241.3 (30MB reduction!)
  )

  # Case 2: True Null Hypothesis (Identical configurations, random variance)
  run_simulation(
      "Case 2: Identical Configurations (Random Variance)",
      baseline=[270.5, 273.1, 269.2, 271.8, 272.0],
      experiment=[270.8, 272.5, 269.8, 271.0, 272.5]  # Extremely close means
  )

  # Case 3: Tiny Optimization drowned by extreme background noise
  run_simulation(
      "Case 3: Small Optimization Drowned by High Process Noise",
      baseline=[270.0, 295.0, 245.0, 285.0, 255.0],
      experiment=[268.0, 293.0, 243.0, 283.0, 253.0])

  print("💡 TIP: Run with --aa-test to execute false positive "
        "A/A calibration runs!")
  print("=" * 55 + "\n")


if __name__ == "__main__":
  main()
