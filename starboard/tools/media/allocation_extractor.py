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
"""Extract allocation operations from a full Cobalt log

The result allocator log is a text file capturing all allocation operations in a
real scenario and can be used by various utilities to verify allocator
implementations.
"""

import os
import re
import sys


# Extract allocation log from the full log file, clean them up, and produce a
# list with one allocation operation per item. Each allocation log line in the
# full log may contain multiple operations in the form of:
#   Media Allocation Log: <index> <operation> <operation> ...
# and each allocation operation is in the form of:
#   a <pointer> <type> <size> <alignment>
#   f <pointer>
# The function extracts these operations and flatten them into a list, without
# cleaning up or verifying.
def ExtractAllocations(full_log_file):
  prefix = 'Media Allocation Log:'

  with open(full_log_file, encoding='utf-8') as f:
    allocation_lines = [
        line[line.find(prefix):].strip() for line in f if line.find(prefix) >= 0
    ]

  # Now the lines are allocation operations in the format of:
  #   Media Allocation Log: <index> <operation> <operation> ...
  #   operation can be:
  #     a <pointer> <type> <size> <alignment>  // for allocation
  #     f <pointer>  // for free
  # e.g.
  #   Media Allocation Log: 1234 a 0x435fe090 1 288 4 f 0xac5630a0
  #   Media Allocation Log: 1236 f 0xac5610a4 f 0xac5600a4

  # Remove the prefix first
  allocation_lines = [
      line.replace('Media Allocation Log:', '').strip()
      for line in allocation_lines
  ]

  current_index = 0
  result = []

  for line in allocation_lines:
    space_position = line.find(' ')

    index = int(line[:space_position])
    assert index == current_index

    payload = line[space_position:].strip()
    pattern = r'[af].*?(?=\s[af]|$)'
    operations = [match.strip() for match in re.findall(pattern, payload)]

    result += operations
    current_index += len(operations)

  return result


# Refines allocation operations in the form of
#   a <pointer> <type> <size> <alignment>
#   f <pointer>
# to
#   allocate <pointer> <type> <size> <alignment>
#   free <pointer> <type> <size>
# This helps reducing the complexity of any further usage.
# The function also verifies the consistency of these operations.
def RefineOperations(allocations):
  refined_allocations = []
  pointer_to_type_and_size = {}

  for allocation in allocations:
    tokens = allocation.split(' ')

    if tokens[0] == 'a':
      assert len(tokens) == 5
      assert tokens[1] not in pointer_to_type_and_size

      pointer_to_type_and_size[tokens[1]] = [tokens[2], tokens[3]]
      tokens[0] = 'allocate'
      refined_allocations.append(' '.join(tokens))
    else:
      assert tokens[0] == 'f'
      assert len(tokens) == 2
      assert tokens[1] in pointer_to_type_and_size

      buffer_type, size = pointer_to_type_and_size[tokens[1]]
      pointer_to_type_and_size.pop(tokens[1])
      refined_allocations.append(' '.join(
          ['free', tokens[1], buffer_type, size]))

  if len(pointer_to_type_and_size) != 0:
    print('Incomplete allocation log with', len(pointer_to_type_and_size),
          'outstanding allocations, filling them up ...')
    for pointer, [buffer_type, size] in pointer_to_type_and_size.items():
      refined_allocations.append(' '.join(['free', pointer, buffer_type, size]))

  return refined_allocations


if len(sys.argv) != 3:
  print('USAGE:', os.path.basename(__file__),
        '<full log path name> <allocation log path name>')
  sys.exit(0)

with open(sys.argv[2], 'w', encoding='utf-8') as output:
  for operation in RefineOperations(ExtractAllocations(sys.argv[1])):
    output.write(f'{operation}\n')
