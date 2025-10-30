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

import unittest
import os
import sys
import tempfile
import shutil
from unittest.mock import patch
from io import StringIO

# Add the script's directory to the Python path to allow importing
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

import analyze_smaps_logs

# Mock data for two points in time
MOCK_PROCESSED_SMAPS_1 = """Processing /path/to/smaps_20251030_010000_1234.txt...
                                              name|         pss|         rss
                                           <lib_A>|        1000|        1500
                                           <lib_B>|         500|         600
                                           <lib_C>|         200|         250
                                             total|        1700|        2350
                                              name|         pss|         rss

Output saved to /path/to/processed/smaps_20251030_010000_1234_processed.txt
"""

MOCK_PROCESSED_SMAPS_2 = """Processing /path/to/smaps_20251030_020000_1234.txt...
                                              name|         pss|         rss
                                           <lib_A>|        1200|        1800
                                           <lib_B>|        1500|        1600
                                           <lib_C>|         200|         250
                                             total|        2900|        3650
                                              name|         pss|         rss

Output saved to /path/to/processed/smaps_20251030_020000_1234_processed.txt
"""

class AnalyzeSmapsLogsTest(unittest.TestCase):
    """Tests for analyze_smaps_logs.py."""

    def setUp(self):
        """Set up a temporary directory and mock files."""
        self.test_dir = tempfile.mkdtemp()
        
        # Create mock files
        with open(os.path.join(self.test_dir, 'smaps_20251030_010000_1234_processed.txt'), 'w') as f:
            f.write(MOCK_PROCESSED_SMAPS_1)
        with open(os.path.join(self.test_dir, 'smaps_20251030_020000_1234_processed.txt'), 'w') as f:
            f.write(MOCK_PROCESSED_SMAPS_2)
        # Create a file with no header to test error handling
        with open(os.path.join(self.test_dir, 'no_header.txt'), 'w') as f:
            f.write("this is not a valid file")

    def tearDown(self):
        """Remove the temporary directory."""
        shutil.rmtree(self.test_dir)

    def test_parse_smaps_file(self):
        """Tests parsing of a single valid processed smaps file."""
        filepath = os.path.join(self.test_dir, 'smaps_20251030_010000_1234_processed.txt')
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
        """Tests the main analysis function and captures its output."""
        analyze_smaps_logs.analyze_logs(self.test_dir)
        output = mock_stdout.getvalue()

        # Check for top consumers in the end log
        self.assertIn("Top 5 Largest Consumers by the End Log (PSS):", output)
        self.assertIn("- <lib_B>: 1500 kB PSS", output)
        self.assertIn("- <lib_A>: 1200 kB PSS", output)

        # Check for memory growth
        self.assertIn("Top 5 Memory Increases Over Time (PSS):", output)
        self.assertIn("- <lib_B>: +1000 kB PSS", output)
        self.assertIn("- <lib_A>: +200 kB PSS", output)

        # Check for overall change
        self.assertIn("Overall Total Memory Change:", output)
        self.assertIn("Total PSS Change: 1200 kB", output)
        self.assertIn("Total RSS Change: 1300 kB", output)

if __name__ == '__main__':
    unittest.main()
