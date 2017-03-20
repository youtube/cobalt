"""Base class for WebDriver tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import math


def mean(list_values):
  """Returns the mean of a list of numeric values.

  Args:
      list_values: A list of numeric values.
  Returns:
      Appropriate value.
  """
  if not list_values:
    return None

  return sum(list_values) / len(list_values)


def percentile(list_values, target_percentile):
  """Returns the percentile of a list.

  This method interpolates between two numbers if the percentile lands between
  two data points.

  Args:
      list_values: A sortable list of values
      target_percentile: A number ranging from 0-100.
  Returns:
      Appropriate value.
  Raises:
    RuntimeError: Raised on invalid args.
  """
  if not list_values:
    return None
  if target_percentile > 100 or target_percentile < 0:
    raise RuntimeError("target_percentile must be 0-100")
  sorted_values = sorted(list_values)

  if target_percentile == 100:
    return sorted_values[-1]
  fractional, index = math.modf(
      (len(sorted_values) - 1) * (target_percentile * 0.01))
  index = int(index)

  if len(sorted_values) == index + 1:
    return sorted_values[index]

  return sorted_values[index] * (1 - fractional
                                ) + sorted_values[index + 1] * fractional


def merge_dict(merge_into, merge_from):
  """Merges the second dict into the first dict.

  Merge into differs from update in that it will not override values.  If the
  values already exist, the resulting value will be a list with a union of
  existing and new items.

  Args:
    merge_into: An output dict to merge values into.
    merge_from: An input dict to iterate over and insert values from.

  Returns:
    None
  """
  if not merge_from:
    return
  for k, v in merge_from.items():
    try:
      existing_value = merge_into[k]
    except KeyError:
      merge_into[k] = v
      continue

    if not isinstance(v, list):
      v = [v]
    if isinstance(existing_value, list):
      existing_value.extend(v)
    else:
      new_value = [existing_value]
      new_value.extend(v)
      merge_into[k] = new_value
