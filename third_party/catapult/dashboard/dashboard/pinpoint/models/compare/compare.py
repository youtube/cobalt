# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import collections

from dashboard.pinpoint.models.compare import kolmogorov_smirnov
from dashboard.pinpoint.models.compare import mann_whitney_u
from dashboard.pinpoint.models.compare import thresholds

DIFFERENT = 'different'
PENDING = 'pending'
SAME = 'same'
UNKNOWN = 'unknown'


class ComparisonResults(
    collections.namedtuple(
        'ComparisonResults',
        ('result', 'p_value', 'low_threshold', 'high_threshold'))):
  __slots__ = ()


# TODO(https://crbug.com/1051710): Make this return all the values useful in
# decision making (and display).
def Compare(values_a, values_b, attempt_count, mode, magnitude):
  """Decide whether two samples are the same, different, or unknown.

  Arguments:
    values_a: A list of sortable values. They don't need to be numeric.
    values_b: A list of sortable values. They don't need to be numeric.
    attempt_count: The average number of attempts made.
    mode: 'functional' or 'performance'. We use different significance
      thresholds for each type.
    magnitude: An estimate of the size of differences to look for. We need more
      values to find smaller differences. If mode is 'functional', this is the
      failure rate, a float between 0 and 1. If mode is 'performance', this is a
      multiple of the interquartile range (IQR).

  Returns:
    A tuple `ComparisonResults` which contains the following elements:
      * result: one of the following values:
          DIFFERENT: The samples are unlikely to come from the same
                     distribution, and are therefore likely different. Reject
                     the null hypothesis.
          SAME     : The samples are unlikely to come from distributions that
                     differ by the given magnitude. Cannot reject the null
                     hypothesis.
          UNKNOWN  : Not enough evidence to reject either hypothesis. We should
                     collect more data before making a final decision.
      * p_value: the consolidated p-value for the statistical tests used in the
                 implementation.
      * low_threshold: the `alpha` where if the p-value is lower means we can
                       reject the null hypothesis.
      * high_threshold: the `alpha` where if the p-value is lower means we need
                        more information to make a definitive judgement.
  """
  low_threshold = thresholds.LowThreshold()
  high_threshold = thresholds.HighThreshold(mode, magnitude, attempt_count)

  if not (len(values_a) > 0 and len(values_b) > 0):
    # A sample has no values in it.
    return ComparisonResults(UNKNOWN, None, low_threshold, high_threshold)

  # MWU is bad at detecting changes in variance, and K-S is bad with discrete
  # distributions. So use both. We want low p-values for the below examples.
  #        a                     b               MWU(a, b)  KS(a, b)
  # [0]*20            [0]*15+[1]*5                0.0097     0.4973
  # range(10, 30)     range(10)+range(30, 40)     0.4946     0.0082
  p_value = min(
      kolmogorov_smirnov.KolmogorovSmirnov(values_a, values_b),
      mann_whitney_u.MannWhitneyU(values_a, values_b))

  if p_value <= low_threshold:
    # The p-value is less than the significance level. Reject the null
    # hypothesis.
    return ComparisonResults(DIFFERENT, p_value, low_threshold, high_threshold)

  if p_value <= thresholds.HighThreshold(mode, magnitude, attempt_count):
    # The p-value is not less than the significance level, but it's small
    # enough to be suspicious. We'd like to investigate more closely.
    return ComparisonResults(UNKNOWN, p_value, low_threshold, high_threshold)

  # The p-value is quite large. We're not suspicious that the two samples might
  # come from different distributions, and we don't care to investigate more.
  return ComparisonResults(SAME, p_value, low_threshold, high_threshold)
