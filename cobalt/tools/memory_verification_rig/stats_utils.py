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
"""Shared statistical calculation helpers for Cobalt significance evaluation."""

import math
import random


def calculate_stats(data):
  """Calculates mean and standard deviation of a numeric array."""
  n = len(data)
  if n == 0:
    return 0.0, 0.0, 0.0
  mean = sum(data) / n
  variance = sum((x - mean)**2 for x in data) / n
  std_dev = math.sqrt(variance)
  return mean, variance, std_dev


def permutation_test_p_value(group1, group2, permutations=10000):
  """Calculates p-value via non-parametric bootstrap permutation test."""
  n1 = len(group1)
  n2 = len(group2)
  if n1 == 0 or n2 == 0:
    return 1.0

  # Observed difference of means
  observed_diff = abs(sum(group1) / n1 - sum(group2) / n2)

  # Pool both groups
  pooled = group1 + group2
  total_len = len(pooled)

  extreme_count = 0
  for _ in range(permutations):
    # Draw randomized permutation indices manually
    # E.g. Knuth shuffle slice representation
    shuffled = pooled.copy()
    # Simple Knuth shuffle step
    for i in range(total_len - 1, 0, -1):
      j = random.randint(0, i)
      shuffled[i], shuffled[j] = shuffled[j], shuffled[i]

    perm_group1 = shuffled[:n1]
    perm_group2 = shuffled[n1:]

    perm_diff = abs(sum(perm_group1) / n1 - sum(perm_group2) / n2)
    if perm_diff >= observed_diff:
      extreme_count += 1

  return extreme_count / permutations
