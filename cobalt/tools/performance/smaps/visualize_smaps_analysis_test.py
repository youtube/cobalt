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
"""Tests for visualize_smaps_analysis.py."""

import json
import unittest
from unittest.mock import patch, mock_open, MagicMock

# The visualize_smaps_analysis module is not in the Python path by default.
# We must add it to the path to ensure that it can be imported.
try:
  from cobalt.tools.performance.smaps import visualize_smaps_analysis
except ImportError:
  # If the script is run from the smaps directory, we need to add the
  # project root to the Python path.
  import os
  import sys
  sys.path.append(
      os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
  from cobalt.tools.performance.smaps import visualize_smaps_analysis

# Sample JSON data for testing, including shared memory regions
regions_t0 = [{
    'name': f'region_{i}',
    'pss': 100,
    'rss': 110
} for i in range(10)]
regions_t0.append({'name': 'mem/shared_memory/A', 'pss': 25, 'rss': 30})
regions_t0.append({'name': 'mem/shared_memory/B', 'pss': 25, 'rss': 30})

regions_t1 = []
for i in range(10):
  pss = 1000 - i * 20
  regions_t1.append({'name': f'region_{i}', 'pss': pss, 'rss': pss + 10})
regions_t1.append({'name': 'mem/shared_memory/A', 'pss': 50, 'rss': 60})
regions_t1.append({'name': 'mem/shared_memory/B', 'pss': 75, 'rss': 80})

SAMPLE_JSON_DATA = [
    {
        'timestamp':
            '20250101_120000',
        'total_memory': {
            'pss': sum(r['pss'] for r in regions_t0) + 50,
            'rss': sum(r['rss'] for r in regions_t0) + 60
        },
        'regions':
            regions_t0 + [{
                'name': '[mem/shared_memory]',
                'pss': 50,
                'rss': 60
            }],
    },
    {
        'timestamp':
            '20250101_120100',
        'total_memory': {
            'pss': sum(r['pss'] for r in regions_t1) + 125,
            'rss': sum(r['rss'] for r in regions_t1) + 140
        },
        'regions':
            regions_t1 + [{
                'name': '[mem/shared_memory]',
                'pss': 125,
                'rss': 140
            }],
    },
]


class TestVisualizeSmapsAnalysis(unittest.TestCase):
  """Unit tests for the smaps visualization script."""

  @patch('matplotlib.pyplot.savefig')
  @patch('matplotlib.pyplot.subplots')
  def test_create_visualization_success(self, mock_subplots, mock_savefig):
    """Test that the main visualization function runs and calls mocks."""
    # Arrange
    mock_fig = MagicMock()
    mock_ax1 = MagicMock()
    mock_ax2 = MagicMock()
    mock_ax3 = MagicMock()
    mock_subplots.return_value = (mock_fig, (mock_ax1, mock_ax2, mock_ax3))

    json_str = json.dumps(SAMPLE_JSON_DATA)
    m = mock_open(read_data=json_str)

    with patch('builtins.open', m):
      # Act
      visualize_smaps_analysis.create_visualization('dummy.json', 'dummy.png')

    # Assert
    # Check that the file was opened for reading
    m.assert_called_once_with('dummy.json', 'r', encoding='utf-8')

    # Check that subplots was called to create the figure
    mock_subplots.assert_called_once()

    # Check that the plot functions were called on the correct axes
    self.assertTrue(mock_ax1.plot.called)

    # Check that stackplot for consumers was called with 10 data series
    self.assertTrue(mock_ax2.stackplot.called)
    args, _ = mock_ax2.stackplot.call_args
    consumer_data = args[1]  # Data is the second positional argument
    self.assertEqual(len(consumer_data), 10)

    # Check that plot for growers was called 10 times
    self.assertEqual(mock_ax3.plot.call_count, 10)

    # Check that the figure was saved
    mock_savefig.assert_called_once_with('dummy.png')

  @patch('cobalt.tools.performance.smaps.visualize_smaps_analysis.'
         'create_visualization')
  @patch('os.path.exists', return_value=True)
  def test_main_function_flow(self, mock_exists, mock_create_viz):
    """Test that the main function calls the visualization function."""
    # Arrange
    test_args = [
        'visualize_smaps_analysis.py',
        'input.json',
        '--output_image',
        'output.png',
    ]

    # Act
    with patch('sys.argv', test_args):
      visualize_smaps_analysis.main()

    # Assert
    mock_exists.assert_any_call('input.json')
    mock_create_viz.assert_called_once_with('input.json', 'output.png')


if __name__ == '__main__':
  unittest.main()
