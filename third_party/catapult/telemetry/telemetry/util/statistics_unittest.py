# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import division
from __future__ import absolute_import
import math
import random
import unittest

from telemetry.util import statistics

# 2To3-division: the / operations here are not converted to // as the results
# are expected floats.

def Relax(samples, iterations=10):
  """Lloyd relaxation in 1D.

  Keeps the position of the first and last sample.
  """
  for _ in range(0, iterations):
    voronoi_boundaries = []
    for i in range(1, len(samples)):
      voronoi_boundaries.append((samples[i] + samples[i-1]) * 0.5)

    relaxed_samples = []
    relaxed_samples.append(samples[0])
    for i in range(1, len(samples)-1):
      relaxed_samples.append(
          (voronoi_boundaries[i-1] + voronoi_boundaries[i]) * 0.5)
    relaxed_samples.append(samples[-1])
    samples = relaxed_samples
  return samples

def CreateRandomSamples(num_samples):
  samples = []
  position = 0.0
  samples.append(position)
  for _ in range(1, num_samples):
    position += random.random()
    samples.append(position)
  return samples

class StatisticsUnitTest(unittest.TestCase):

  def testNormalizeSamples(self):
    samples = []
    normalized_samples, scale = statistics.NormalizeSamples(samples)
    self.assertEqual(normalized_samples, [])
    self.assertEqual(scale, 1.0)

    samples = [0.0, 0.0]
    normalized_samples, scale = statistics.NormalizeSamples(samples)
    self.assertEqual(normalized_samples, [0.5, 0.5])
    self.assertEqual(scale, 1.0)

    samples = [0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0]
    normalized_samples, scale = statistics.NormalizeSamples(samples)
    self.assertEqual(normalized_samples,
                     [1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0])
    self.assertEqual(scale, 0.75)

    samples = [1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0]
    normalized_samples, scale = statistics.NormalizeSamples(samples)
    self.assertEqual(normalized_samples, samples)
    self.assertEqual(scale, 1.0)

  def testDiscrepancyRandom(self):
    """Tests NormalizeSamples and Discrepancy with random samples.

    Generates 10 sets of 10 random samples, computes the discrepancy,
    relaxes the samples using Llloyd's algorithm in 1D, and computes the
    discrepancy of the relaxed samples. Discrepancy of the relaxed samples
    must be less than or equal to the discrepancy of the original samples.
    """
    random.seed(1234567)
    for _ in range(0, 10):
      samples = CreateRandomSamples(10)
      samples = statistics.NormalizeSamples(samples)[0]
      d = statistics.Discrepancy(samples)
      relaxed_samples = Relax(samples)
      d_relaxed = statistics.Discrepancy(relaxed_samples)
      self.assertTrue(d_relaxed <= d)

  def testDiscrepancyAnalytic(self):
    """Computes discrepancy for sample sets with known statistics."""
    samples = []
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.0)

    samples = [0.5]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.5)

    samples = [0.0, 1.0]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 1.0)

    samples = [0.5, 0.5, 0.5]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 1.0)

    samples = [1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.25)

    samples = [1.0 / 8.0, 5.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.5)

    samples = [1.0 / 8.0, 3.0 / 8.0, 5.0 / 8.0, 5.0 / 8.0, 7.0 / 8.0]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.4)

    samples = [0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.5)

    samples = statistics.NormalizeSamples(samples)[0]
    d = statistics.Discrepancy(samples)
    self.assertEqual(d, 0.25)

  def testTimestampsDiscrepancy(self):
    time_stamps = []
    d_abs = statistics.TimestampsDiscrepancy(time_stamps, True)
    self.assertEqual(d_abs, 0.0)

    time_stamps = [4]
    d_abs = statistics.TimestampsDiscrepancy(time_stamps, True)
    self.assertEqual(d_abs, 0.5)

    time_stamps_a = [0, 1, 2, 3, 5, 6]
    time_stamps_b = [0, 1, 2, 3, 5, 7]
    time_stamps_c = [0, 2, 3, 4]
    time_stamps_d = [0, 2, 3, 4, 5]

    d_abs_a = statistics.TimestampsDiscrepancy(time_stamps_a, True)
    d_abs_b = statistics.TimestampsDiscrepancy(time_stamps_b, True)
    d_abs_c = statistics.TimestampsDiscrepancy(time_stamps_c, True)
    d_abs_d = statistics.TimestampsDiscrepancy(time_stamps_d, True)
    d_rel_a = statistics.TimestampsDiscrepancy(time_stamps_a, False)
    d_rel_b = statistics.TimestampsDiscrepancy(time_stamps_b, False)
    d_rel_c = statistics.TimestampsDiscrepancy(time_stamps_c, False)
    d_rel_d = statistics.TimestampsDiscrepancy(time_stamps_d, False)

    self.assertTrue(d_abs_a < d_abs_b)
    self.assertTrue(d_rel_a < d_rel_b)
    self.assertTrue(d_rel_d < d_rel_c)
    self.assertAlmostEqual(d_abs_d, d_abs_c)

  def testDiscrepancyMultipleRanges(self):
    samples = [[0.0, 1.2, 2.3, 3.3], [6.3, 7.5, 8.4], [4.2, 5.4, 5.9]]
    d_0 = statistics.TimestampsDiscrepancy(samples[0])
    d_1 = statistics.TimestampsDiscrepancy(samples[1])
    d_2 = statistics.TimestampsDiscrepancy(samples[2])
    d = statistics.TimestampsDiscrepancy(samples)
    self.assertEqual(d, max(d_0, d_1, d_2))

  def testApproximateDiscrepancy(self):
    """Tests approimate discrepancy implementation by comparing to exact
    solution.
    """
    random.seed(1234567)
    for _ in range(0, 5):
      samples = CreateRandomSamples(10)
      samples = statistics.NormalizeSamples(samples)[0]
      d = statistics.Discrepancy(samples)
      d_approx = statistics.Discrepancy(samples, 500)
      self.assertEqual(round(d, 2), round(d_approx, 2))

  def testPercentile(self):
    # The 50th percentile is the median value.
    self.assertEqual(3, statistics.Percentile([4, 5, 1, 3, 2], 50))
    self.assertEqual(2.5, statistics.Percentile([5, 1, 3, 2], 50))
    # When the list of values is empty, 0 is returned.
    self.assertEqual(0, statistics.Percentile([], 50))
    # When the given percentage is very low, the lowest value is given.
    self.assertEqual(1, statistics.Percentile([2, 1, 5, 4, 3], 5))
    # When the given percentage is very high, the highest value is given.
    self.assertEqual(5, statistics.Percentile([5, 2, 4, 1, 3], 95))
    # Linear interpolation between closest ranks is used. Using the example
    # from <http://en.wikipedia.org/wiki/Percentile>:
    self.assertEqual(27.5, statistics.Percentile([15, 20, 35, 40, 50], 40))

  def testArithmeticMean(self):
    # The ArithmeticMean function computes the simple average.
    self.assertAlmostEqual(40 / 3.0, statistics.ArithmeticMean([10, 10, 20]))
    self.assertAlmostEqual(15.0, statistics.ArithmeticMean([10, 20]))
    # If the 'count' is zero, then zero is returned.
    self.assertEqual(0, statistics.ArithmeticMean([]))

  def testStandardDeviation(self):
    self.assertAlmostEqual(math.sqrt(2 / 3.0),
                           statistics.StandardDeviation([1, 2, 3]))
    self.assertEqual(0, statistics.StandardDeviation([1]))
    self.assertEqual(0, statistics.StandardDeviation([]))

  def testTrapezoidalRule(self):
    self.assertEqual(4, statistics.TrapezoidalRule([1, 2, 3], 1))
    self.assertEqual(2, statistics.TrapezoidalRule([1, 2, 3], .5))
    self.assertEqual(0, statistics.TrapezoidalRule([1, 2, 3], 0))
    self.assertEqual(-4, statistics.TrapezoidalRule([1, 2, 3], -1))
    self.assertEqual(3, statistics.TrapezoidalRule([-1, 2, 3], 1))
    self.assertEqual(0, statistics.TrapezoidalRule([1], 1))
    self.assertEqual(0, statistics.TrapezoidalRule([0], 1))
