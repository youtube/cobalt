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
"""Tests for run_analysis_pipeline.py."""

import os
import shutil
import tempfile
import unittest
from unittest.mock import patch

import run_analysis_pipeline

# Sample smaps data for testing
SAMPLE_SMAPS_1 = """7f6d5c000000-7f6d5c021000 r-xp 00000000 08:01 12345 \
/lib/x86_64-linux-gnu/ld-2.27.so
VmPeak:            132 kB
Size:                132 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
Rss:                  84 kB
Pss:                  84 kB
Shared_Clean:          0 kB
Shared_Dirty:          0 kB
Private_Clean:        84 kB
Private_Dirty:         0 kB
Referenced:           84 kB
Anonymous:             0 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                 10 kB
SwapPss:               5 kB
Locked:                0 kB
VmFlags: rd ex mr mw me dw
"""


class RunAnalysisPipelineTest(unittest.TestCase):
  """Tests for run_analysis_pipeline.py."""

  def setUp(self):
    """Set up a temporary directory and sample files for testing."""
    self.test_dir = tempfile.mkdtemp()
    self.raw_logs_dir = os.path.join(self.test_dir, 'raw_logs')
    os.makedirs(self.raw_logs_dir)

    self.smaps_file_1 = os.path.join(self.raw_logs_dir, 'smaps1.txt')

    with open(self.smaps_file_1, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_1)

  def tearDown(self):
    """Remove the temporary directory after tests."""
    shutil.rmtree(self.test_dir)

  @patch('run_analysis_pipeline.create_visualization')
  @patch('run_analysis_pipeline.run_smaps_analysis_tool')
  @patch('run_analysis_pipeline.run_smaps_batch_tool')
  def test_pipeline_execution(self, mock_batch_tool, mock_analysis_tool,
                              mock_viz_tool):
    """Tests that the pipeline runs the tools in the correct order (Android)."""
    output_image = os.path.join(self.test_dir, 'output.png')
    test_args = [
        self.raw_logs_dir, '--output_image', output_image, '--platform',
        'android'
    ]

    with patch('sys.argv', ['run_analysis_pipeline.py'] + test_args):
      run_analysis_pipeline.main()

    # Verify that each tool was called once
    mock_batch_tool.assert_called_once_with([
        self.raw_logs_dir, '-o', unittest.mock.ANY, '--platform', 'android',
        '-d'
    ])
    mock_analysis_tool.assert_called_once()
    mock_viz_tool.assert_called_once()

    # Verify the arguments passed to the visualization tool
    args, _ = mock_viz_tool.call_args
    self.assertTrue(args[0].endswith('analysis.json'))
    self.assertEqual(args[1], output_image)

  @patch('run_analysis_pipeline.create_visualization')
  @patch('run_analysis_pipeline.run_smaps_analysis_tool')
  @patch('run_analysis_pipeline.run_smaps_batch_tool')
  def test_pipeline_execution_linux(self, mock_batch_tool, mock_analysis_tool,
                                    mock_viz_tool):
    """Tests that the pipeline runs correctly for Linux platform."""
    output_image = os.path.join(self.test_dir, 'output_linux.png')
    test_args = [
        self.raw_logs_dir, '--output_image', output_image, '--platform', 'linux'
    ]

    with patch('sys.argv', ['run_analysis_pipeline.py'] + test_args):
      run_analysis_pipeline.main()

    # Verify that each tool was called once
    # Note: -d (aggregate_android) should NOT be present for linux platform
    mock_batch_tool.assert_called_once_with(
        [self.raw_logs_dir, '-o', unittest.mock.ANY, '--platform', 'linux'])
    mock_analysis_tool.assert_called_once()
    mock_viz_tool.assert_called_once()

    # Verify the arguments passed to the visualization tool
    args, _ = mock_viz_tool.call_args
    self.assertTrue(args[0].endswith('analysis.json'))
    self.assertEqual(args[1], output_image)


if __name__ == '__main__':
  unittest.main()
