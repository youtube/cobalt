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
Swap:                 10 kB
SwapPss:               5 kB
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
Swap:                  2 kB
SwapPss:               1 kB
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

# Sample smaps data with Android-specific regions for testing aggregation
SAMPLE_SMAPS_ANDROID = """12c00000-12e00000 rw-p 00000000 00:00 0    \
    [anon:scudo:primary]
Size:               2048 kB
Rss:                1024 kB
Pss:                1024 kB
Private_Dirty:      1024 kB
Swap:                  0 kB
SwapPss:               0 kB
VmFlags: rd wr mr mw me ac
7f6d5c022000-7f6d5c023000 rw-p 00000000 00:00 0    /dev/ashmem/bitmap
Size:                  4 kB
Rss:                   4 kB
Pss:                   4 kB
Private_Dirty:         4 kB
Swap:                  0 kB
SwapPss:               0 kB
VmFlags: rd wr mr mw me ac
7f6d5c023000-7f6d5c024000 rw-p 00000000 00:00 0    /memfd:jit-cache
Size:                  4 kB
Rss:                   4 kB
Pss:                   4 kB
Private_Dirty:         4 kB
Swap:                  0 kB
SwapPss:               0 kB
VmFlags: rd wr mr mw me ac
"""

# Sample smaps data with Linux-specific regions for testing aggregation
SAMPLE_SMAPS_LINUX = """12c00000-12e00000 rw-p 00000000 00:00 0    [heap]
Size:               2048 kB
Rss:                1024 kB
Pss:                1024 kB
Private_Dirty:      1024 kB
Swap:                  0 kB
SwapPss:               0 kB
VmFlags: rd wr mr mw me ac
7f6d5c022000-7f6d5c023000 rw-p 00000000 00:00 0    /dev/zero (deleted)
Size:                  4 kB
Rss:                   4 kB
Pss:                   4 kB
Private_Dirty:         4 kB
Swap:                  0 kB
SwapPss:               0 kB
VmFlags: rd wr mr mw me ac
7f6d5c023000-7f6d5c024000 rw-p 00000000 00:00 0    [stack]
Size:                  4 kB
Rss:                   4 kB
Pss:                   4 kB
Private_Dirty:         4 kB
Swap:                  0 kB
SwapPss:               0 kB
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
    self.smaps_file_android = os.path.join(self.input_dir, 'smaps_android.txt')
    self.smaps_file_linux = os.path.join(self.input_dir, 'smaps_linux.txt')

    with open(self.smaps_file_1, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_1)
    with open(self.smaps_file_2, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_2)
    with open(self.smaps_file_android, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_ANDROID)
    with open(self.smaps_file_linux, 'w', encoding='utf-8') as f:
      f.write(SAMPLE_SMAPS_LINUX)

  def tearDown(self):
    """Remove the temporary directory after tests."""
    shutil.rmtree(self.test_dir)

  def test_single_file_processing(self):
    """Tests processing a single smaps file."""
    output_file = os.path.join(self.output_dir, 'smaps1_processed.txt')
    test_args = [self.input_dir, '-o', self.output_dir]

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
    test_args = [self.input_dir, '-o', self.output_dir]

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
    test_args = [self.input_dir, '-o', self.output_dir, '-a']

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file))
    with open(output_file, 'r', encoding='utf-8') as f:
      content = f.read()
      # Check that the library is aggregated into <dynlibs>
      self.assertIn('<dynlibs>', content)
      self.assertNotIn('/lib/x86_64-linux-gnu/ld-2.27.so', content)

  def test_android_aggregation(self):
    """Tests that Android-specific aggregation rules are applied."""
    output_file = os.path.join(self.output_dir, 'smaps_android_processed.txt')
    test_args = [
        self.input_dir, '-o', self.output_dir, '--platform', 'android', '-d'
    ]

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file))
    with open(output_file, 'r', encoding='utf-8') as f:
      content = f.read()
      self.assertIn('<anon:scudo:primary>', content)
      self.assertIn('</dev/ashmem/bitmap>', content)
      self.assertIn('</memfd:jit-cache>', content)

  def test_linux_aggregation(self):
    """Tests that Android-specific aggregation rules are
       NOT applied for Linux."""
    output_file = os.path.join(self.output_dir, 'smaps_linux_processed.txt')
    test_args = [self.input_dir, '-o', self.output_dir, '--platform', 'linux']

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file))
    with open(output_file, 'r', encoding='utf-8') as f:
      content = f.read()
      # For Linux, these should NOT be aggregated
      self.assertIn('[heap]', content)
      self.assertIn('/dev/zero (deleted)', content)
      self.assertIn('[stack]', content)
      self.assertNotIn('<heap>', content)
      self.assertNotIn('</dev/zero (deleted)>', content)
      self.assertNotIn('<stack>', content)

  def test_swap_fields_present(self):
    """Tests that swap and swap_pss fields are in the output."""
    output_file = os.path.join(self.output_dir, 'smaps1_processed.txt')
    test_args = [self.input_dir, '-o', self.output_dir]

    read_smaps_batch.run_smaps_batch_tool(test_args)

    self.assertTrue(os.path.exists(output_file))
    with open(output_file, 'r', encoding='utf-8') as f:
      content = f.read()
      self.assertIn('swap', content)
      self.assertIn('swap_pss', content)
      # Check that the total swap is the sum of the swap values in the sample
      self.assertRegex(content, r'total.*12.*6')


if __name__ == '__main__':
  unittest.main()
