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
"""Tests for analyze_smaps_logs.py."""

from io import StringIO
import os
import shutil
import tempfile
import unittest
from unittest.mock import patch
import json

import analyze_smaps_logs

# Mock data for two points in time
MOCK_PROCESSED_SMAPS_1 = """Processing
/path/to/smaps_20251030_010000_1234.txt...
                                              name|         pss|         rss
                                           <lib_A>|        1000|        1500
                                           <lib_B>|         500|         600
                                           <lib_C>|         200|         250
                                                                                           total|        1700|        2350
                                                                                           name|         pss|         rss

                                             Output saved to
                                             /path/to/processed/smaps_20251030_010000_1234_processed.txt
                                             """

MOCK_PROCESSED_SMAPS_2 = """Processing
/path/to/smaps_20251030_020000_1234.txt...
                                              name|         pss|         rss
                                           <lib_A>|        1200|        1800
                                           <lib_B>|        1500|        1600
                                           <lib_C>|         200|         250
                                             total|        2900|        3650
                                              name|         pss|         rss

Output saved to
/path/to/processed/smaps_20251030_020000_1234_processed.txt
"""

# Mock data for various edge cases
MOCK_SMAPS_NO_GROWTH_1 = """
name|pss|rss
<lib_A>|1000|1500
<lib_B>|500|600
total|1500|2100
"""

MOCK_SMAPS_NO_GROWTH_2 = """
name|pss|rss
<lib_A>|900|1400
<lib_B>|500|600
total|1400|2000
"""

MOCK_SMAPS_NEW_REGION_1 = """
name|pss|rss
<lib_A>|1000|1500
total|1000|1500
"""

MOCK_SMAPS_NEW_REGION_2 = """
name|pss|rss
<lib_A>|1000|1500
<lib_C>|300|400
total|1300|1900
"""

MOCK_SMAPS_DISAPPEARING_REGION_1 = """
name|pss|rss
<lib_A>|1000|1500
<lib_D>|700|800
total|1700|2300
"""

MOCK_SMAPS_DISAPPEARING_REGION_2 = """
name|pss|rss
<lib_A>|1100|1600
total|1100|1600
"""

MOCK_SMAPS_MIXED_GROWTH_1 = """
name|pss|rss
<lib_A>|1000|1000
<lib_B>|1000|1000
<lib_C>|1000|1000
total|3000|3000
"""

MOCK_SMAPS_MIXED_GROWTH_2 = """
name|pss|rss
<lib_A>|800|800
<lib_B>|1500|1500
<lib_C>|1200|1200
total|3500|3500
"""

# Mock data for a memory leak scenario over three points in time
MOCK_LEAK_SMAPS_1 = """
name|pss|rss
<leaking_lib>|1000|1200
<stable_lib>|500|500
total|1500|1700
"""

MOCK_LEAK_SMAPS_2 = """
name|pss|rss
<leaking_lib>|2000|2500
<stable_lib>|500|500
total|2500|3000
"""

MOCK_LEAK_SMAPS_3 = """
name|pss|rss
<leaking_lib>|3000|3800
<stable_lib>|500|500
total|3500|4300
"""


class AnalyzeSmapsLogsTest(unittest.TestCase):
  """Tests for analyze_smaps_logs.py."""

  def setUp(self):
    """Set up a temporary directory and mock files."""
    self.test_dir = tempfile.mkdtemp()

    # Create mock files for standard tests
    with open(
        os.path.join(self.test_dir, 'smaps_20251030_010000_1234_processed.txt'),
        'w',
        encoding='utf-8') as f:
      f.write(MOCK_PROCESSED_SMAPS_1)
    with open(
        os.path.join(self.test_dir, 'smaps_20251030_020000_1234_processed.txt'),
        'w',
        encoding='utf-8') as f:
      f.write(MOCK_PROCESSED_SMAPS_2)

    # Create a file with no header to test error handling
    with open(
        os.path.join(self.test_dir, 'no_header.txt'), 'w',
        encoding='utf-8') as f:
      f.write('this is not a valid file')

  def tearDown(self):
    """Remove the temporary directory."""
    shutil.rmtree(self.test_dir)

  def test_extract_timestamp_android_format(self):
    """Tests timestamp extraction from Android-style filenames."""
    filename = 'smaps_20251105_102030_12345_processed.txt'
    timestamp = analyze_smaps_logs.extract_timestamp(filename)
    self.assertEqual(timestamp, '20251105_102030')

  def test_extract_timestamp_linux_format(self):
    """Tests timestamp extraction from Linux-style filenames."""
    filename = 'smaps_20251105_192832_cobalt_processed.txt'
    timestamp = analyze_smaps_logs.extract_timestamp(filename)
    self.assertEqual(timestamp, '20251105_192832')

  def test_parse_smaps_file(self):
    """Tests parsing of a single valid processed smaps file."""
    filepath = os.path.join(self.test_dir,
                            'smaps_20251030_010000_1234_processed.txt')
    memory_data, total_data = analyze_smaps_logs.parse_smaps_file(filepath)

    self.assertIsNotNone(memory_data)
    self.assertIsNotNone(total_data)
    self.assertIn('<lib_A>', memory_data)
    self.assertEqual(memory_data['<lib_A>']['pss'], 1000)
    self.assertEqual(total_data['pss'], 1700)

  def test_parse_invalid_file(self):
    """Tests that parsing a file without a header raises a ParsingError."""
    filepath = os.path.join(self.test_dir, 'no_header.txt')
    with self.assertRaises(analyze_smaps_logs.ParsingError):
      analyze_smaps_logs.parse_smaps_file(filepath)

  @patch('sys.stdout', new_callable=StringIO)
  def test_analyze_logs_output(self, mock_stdout):
    """Tests the main analysis function's output."""
    analyze_smaps_logs.analyze_logs(self.test_dir)
    output = mock_stdout.getvalue()

    # Check for the Top 10 consumers sections
    self.assertIn('Top 10 Largest Consumers by PSS:', output)
    self.assertIn('- <lib_B>: 1500 kB PSS', output)
    self.assertIn('- <lib_A>: 1200 kB PSS', output)

    self.assertIn('Top 10 Largest Consumers by RSS:', output)
    self.assertIn('- <lib_A>: 1800 kB RSS', output)
    self.assertIn('- <lib_B>: 1600 kB RSS', output)

    # Check for the detailed memory region output from the *last* log
    self.assertIn('Full Memory Breakdown:', output)
    self.assertIn('- <lib_A>: 1200 kB PSS, 1800 kB RSS', output)
    self.assertIn('- <lib_B>: 1500 kB PSS, 1600 kB RSS', output)

    # Check for the total memory summary
    self.assertIn('Total Memory:', output)
    self.assertIn('- PSS: 2900 kB', output)

  @patch('sys.stdout', new_callable=StringIO)
  def test_analyze_logs_empty_directory(self, mock_stdout):
    """Tests analyze_logs with an empty directory."""
    empty_dir = tempfile.mkdtemp()
    analyze_smaps_logs.analyze_logs(empty_dir)
    output = mock_stdout.getvalue()
    self.assertIn(
        f'No processed smaps files with valid timestamps found in '
        f'{empty_dir}', output)
    shutil.rmtree(empty_dir)

  @patch('sys.stdout', new_callable=StringIO)
  def test_analyze_logs_no_valid_timestamps(self, mock_stdout):
    """Tests analyze_logs with files that have no valid timestamps."""
    invalid_ts_dir = tempfile.mkdtemp()
    with open(
        os.path.join(invalid_ts_dir, 'smaps_invalid_processed.txt'),
        'w',
        encoding='utf-8') as f:
      f.write(MOCK_PROCESSED_SMAPS_1)
    analyze_smaps_logs.analyze_logs(invalid_ts_dir)
    output = mock_stdout.getvalue()
    self.assertIn(
        f'No processed smaps files with valid timestamps found in '
        f'{invalid_ts_dir}', output)
    shutil.rmtree(invalid_ts_dir)

  def test_json_output_for_leak_scenario(self):
    """Tests that the JSON output correctly captures a memory leak scenario."""
    # Create a dedicated directory for this test to avoid conflicts
    leak_test_dir = os.path.join(self.test_dir, 'leak_test')
    os.makedirs(leak_test_dir)

    # Write mock files representing a memory leak
    with open(
        os.path.join(leak_test_dir, 'smaps_20251101_100000_0000_processed.txt'),
        'w',
        encoding='utf-8') as f:
      f.write(MOCK_LEAK_SMAPS_1)
    with open(
        os.path.join(leak_test_dir, 'smaps_20251101_110000_0000_processed.txt'),
        'w',
        encoding='utf-8') as f:
      f.write(MOCK_LEAK_SMAPS_2)
    with open(
        os.path.join(leak_test_dir, 'smaps_20251101_120000_0000_processed.txt'),
        'w',
        encoding='utf-8') as f:
      f.write(MOCK_LEAK_SMAPS_3)

    json_output_path = os.path.join(self.test_dir, 'analysis.json')
    analyze_smaps_logs.analyze_logs(leak_test_dir, json_output_path)

    # Verify the JSON output
    self.assertTrue(os.path.exists(json_output_path))
    with open(json_output_path, 'r', encoding='utf-8') as f:
      data = json.load(f)

    # Check that all three snapshots are present
    self.assertIsInstance(data, list)
    self.assertEqual(len(data), 3)

    # Check timestamps
    self.assertEqual(data[0]['timestamp'], '20251101_100000')
    self.assertEqual(data[1]['timestamp'], '20251101_110000')
    self.assertEqual(data[2]['timestamp'], '20251101_120000')

    # Find the leaking library in each snapshot and verify its growth
    pss_trend = []
    rss_trend = []
    for snapshot in data:
      leaking_lib_data = next(
          (r for r in snapshot['regions'] if r['name'] == '<leaking_lib>'),
          None)
      self.assertIsNotNone(leaking_lib_data)
      pss_trend.append(leaking_lib_data['pss'])
      rss_trend.append(leaking_lib_data['rss'])

    # Assert that PSS and RSS are strictly increasing
    self.assertEqual(pss_trend, [1000, 2000, 3000])
    self.assertEqual(rss_trend, [1200, 2500, 3800])

    # Verify total PSS is also tracked correctly
    total_pss_trend = [s['total_memory']['pss'] for s in data]
    self.assertEqual(total_pss_trend, [1500, 2500, 3500])


if __name__ == '__main__':
  unittest.main()
