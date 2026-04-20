#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Tests for interpret_uma_histogram.py."""

import unittest
from unittest.mock import patch, mock_open
import interpret_uma_histogram


class InterpretUmaHistogramTest(unittest.TestCase):
  """Tests for interpret_uma_histogram.py."""

  def test_calculate_percentiles(self):
    """Tests that percentiles are calculated correctly."""
    histogram = {
        'count':
            100,
        'buckets': [
            {
                'low': 0,
                'high': 10,
                'count': 25
            },
            {
                'low': 10,
                'high': 20,
                'count': 25
            },
            {
                'low': 20,
                'high': 30,
                'count': 25
            },
            {
                'low': 30,
                'high': 40,
                'count': 25
            },
        ]
    }
    percentiles = interpret_uma_histogram.calculate_percentiles(histogram)
    self.assertEqual(percentiles[25], 10)
    self.assertEqual(percentiles[50], 20)
    self.assertEqual(percentiles[75], 30)
    self.assertEqual(percentiles[95], 40)
    self.assertEqual(percentiles[99], 40)

  def test_calculate_percentiles_empty(self):
    """Tests that None is returned for empty histograms."""
    histogram = {'count': 0, 'buckets': []}
    percentiles = interpret_uma_histogram.calculate_percentiles(histogram)
    for p in [25, 50, 75, 95, 99]:
      self.assertIsNone(percentiles[p])

  @patch('interpret_uma_histogram.plot_histogram_data')
  @patch('sys.argv', ['', 'test_data.txt', '--visualize'])
  def test_main_logic(self, mock_plot_histogram_data):
    """Tests the main logic of the script."""
    mock_file_content = (
        '2025-11-06 12:00:00,Query.A,{"result": {"histograms": ['
        '{"name": "Hist.A1", "count": 10, "sum": 50, "buckets": []},'
        '{"name": "Hist.A2", "count": 20, "sum": 150, "buckets": []}'
        ']}}\n'
        '2025-11-06 12:00:00,Query.B,{"result": {"histograms": ['
        '{"name": "Hist.B1", "count": 0, "sum": 0, "buckets": []}'
        ']}}')
    with patch('builtins.open', mock_open(read_data=mock_file_content)):
      interpret_uma_histogram.main()

    # Verify that plot_histogram_data was called.
    self.assertTrue(mock_plot_histogram_data.called)
    # Get the arguments passed to the mock.
    call_args = mock_plot_histogram_data.call_args[0][0]

    # Check that Hist.A1 and Hist.A2 are in the data, but Hist.B1 is not.
    self.assertIn('Hist.A1', call_args)
    self.assertIn('Hist.A2', call_args)
    self.assertNotIn('Hist.B1', call_args)
    # Check that the data for Hist.A1 is correct.
    self.assertEqual(len(call_args['Hist.A1']), 1)


if __name__ == '__main__':
  unittest.main()
