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
"""Tests for read_smaps_batch.py."""

import os
import shutil
import tempfile
import unittest

import read_smaps_batch

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
Swap:                  0 kB
SwapPss:               0 kB
Locked:                0 kB
VmFlags: rd ex mr mw me dw
7f6d5c021000-7f6d5c022000 rw-p 00021000 08:01 12345 \
/lib/x86_64-linux-gnu/ld-2.27.so
VmPeak:              4 kB
Size:                  4 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
Rss:                   4 kB
Pss:                   4 kB
Shared_Clean:          0 kB
Shared_Dirty:          0 kB
Private_Clean:         0 kB
Private_Dirty:         4 kB
Referenced:            4 kB
Anonymous:             4 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                  0 kB
SwapPss:               0 kB
Locked:                0 kB
VmFlags: rd wr mr mw me dw
"""

SAMPLE_SMAPS_2 = """7f6d5c022000-7f6d5c023000 rw-p 00000000 00:00 0    [heap]
VmPeak:              4 kB
Size:                  4 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
Rss:                   4 kB
Pss:                   4 kB
Shared_Clean:          0 kB
Shared_Dirty:          0 kB
Private_Clean:         0 kB
Private_Dirty:         4 kB
Referenced:            4 kB
Anonymous:             4 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                  0 kB
SwapPss:               0 kB
Locked:                0 kB
VmFlags: rd wr mr mw me ac
"""


class ReadSmapsTest(unittest.TestCase):
  """Tests for read_smaps_batch.py."""

  def setUp(self):
    """Set up a temporary directory and sample files for testing."""
    self.test_dir = tempfile.mkdtemp()
    self.input_dir = os.path.join(self.test_dir, 'input')
    self.output_dir = os.path.join(self.test_dir, 'output')
    os.makedirs(self.input_dir)

    self.smaps_file_1 = os.path.join(self.input_dir, 'smaps1.txt')
    self.smaps_file_2 = os.path.join(self.input_dir, 'smaps2.txt')

    with open(self.smaps_file_1, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_1)
    with open(self.smaps_file_2, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_2)

  def tearDown(self):
    """Remove the temporary directory after tests."""
    shutil.rmtree(self.test_dir)

  def test_single_file_processing(self):
    """Tests processing a single smaps file."""
    output_file = os.path.join(self.output_dir, 'smaps1_processed.txt')
    test_args = [self.smaps_file_1, '-o', self.output_dir]

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file))
    with open(output_file, 'r', encoding='utf-8') as f:
      content = f.read()
      self.assertIn('/lib/x86_64-linux-gnu/ld-2.27.so', content)
      self.assertIn('total', content)

  def test_multi_file_processing(self):
    """Tests processing multiple smaps files."""
    output_file_1 = os.path.join(self.output_dir, 'smaps1_processed.txt')
    output_file_2 = os.path.join(self.output_dir, 'smaps2_processed.txt')
    test_args = [
        self.smaps_file_1, self.smaps_file_2, '-o', self.output_dir
    ]

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file_1))
    self.assertTrue(os.path.exists(output_file_2))

    with open(output_file_1, 'r', encoding='utf-8') as f:
      self.assertIn('/lib/x86_64-linux-gnu/ld-2.27.so', f.read())
    with open(output_file_2, 'r', encoding='utf-8') as f:
      self.assertIn('[heap]', f.read())

  def test_aggregation_argument(self):
    """Tests that aggregation arguments are applied."""
    output_file = os.path.join(self.output_dir, 'smaps1_processed.txt')
    test_args = [self.smaps_file_1, '-o', self.output_dir, '-a']

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file))
    with open(output_file, 'r', encoding='utf-8') as f:
      content = f.read()
      # Check that the library is aggregated into <dynlibs>
      self.assertIn('<dynlibs>', content)
      self.assertNotIn('/lib/x86_64-linux-gnu/ld-2.27.so', content)


if __name__ == '__main__':
  unittest.main()
