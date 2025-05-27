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

import os

SECOND = 1000000  # in microseconds
INTERVAL = 5 * SECOND


def ShouldIgnoreCallstack(symbolized_stack):
  return (symbolized_stack.find('CpuVideoFrame::CreateYV12Frame()') >= 0 or
          symbolized_stack.find('CpuVideoFrame::ConvertTo()') >= 0 or
          symbolized_stack.find('dav1d') >= 0 or
          symbolized_stack.find('ReuseAllocatorBase::ExpandToFit()') >= 0)


def SymbolizeStack(symbols, stack):
  if len(symbols):
    return ','.join([symbols.get(address, address) for address in stack[:10]])
  return ''


def LogInterval(program_start, interval_start, interval_end, symbols,
                allocations):
  print('======== interval', interval_start - program_start, '..',
        interval_end - program_start, '========')

  total_size = 0
  total_allocations = 0

  for address, info in allocations.items():
    address = address  # ignore
    timestamp, size, stack = info

    symbolized_stack = SymbolizeStack(symbols, stack)
    if not ShouldIgnoreCallstack(symbolized_stack):
      total_size += size
      total_allocations += 1
      if size > 512 * 1024:
        print(timestamp - program_start, size, symbolized_stack)

  print('total', total_allocations, 'allocations in', total_size, 'bytes')


def LogAllocations(symbols, allocations):
  print('======== allocations ========')

  keyed_allocations = {}
  total_size = 0
  total_allocations = 0

  for address, info in allocations.items():
    address = address  # ignore
    timestamp, size, stack = info

    symbolized_stack = SymbolizeStack(symbols, stack)
    if ShouldIgnoreCallstack(symbolized_stack):
      continue

    total_size += size
    total_allocations += 1

    if symbolized_stack in keyed_allocations:
      keyed_allocations[symbolized_stack].append(size)
    else:
      keyed_allocations[symbolized_stack] = [size]

  sorted_allocations = sorted(
      keyed_allocations.items(), key=lambda item: sum(item[1]))
  for stack, sizes in sorted_allocations:
    if sum(sizes) > 512 * 1024:
      print(
          len(sizes), 'allocations with', sum(sizes), 'bytes, range',
          min(sizes), '..', max(sizes), stack)

  print('total', total_allocations, 'allocations in', total_size, 'bytes')


def AnalyzeFile(allocations_filename, symbols_filename):
  allocations = {}
  program_start = -1
  interval_start = -1
  line_no = 0
  symbols = {}

  if os.path.exists(symbols_filename):
    with open(symbols_filename) as f:
      for line in f:
        tokens = line.split()
        if len(tokens) != 2:
          continue
        symbols[tokens[0]] = tokens[1]

  with open(allocations_filename) as f:
    for line in f:
      line_no += 1

      tokens = line.split()
      # token is 'index timestamp operation address [size stack-trace...]'
      if tokens[2] == 'a' or tokens[2] == 'f':
        timestamp = int(tokens[1])
        operation = tokens[2]
        address = tokens[3]
        size_index = 4
      else:
        timestamp = int(tokens[1])
        operation = tokens[3]
        address = tokens[4]
        size_index = 5
        print('encountering one corrupted line', tokens[0], line_no)

      if program_start == -1:
        program_start = timestamp
        interval_start = timestamp

      if timestamp > interval_start + INTERVAL:
        # LogInterval(program_start, interval_start, interval_start + INTERVAL,
        #             symbols, allocations)
        interval_start += INTERVAL

      if operation == 'a':
        if address in allocations:
          print(address, 'already allocated on line', tokens[0],
                SymbolizeStack(symbols, tokens[size_index + 1:]))
          del allocations[address]
        allocations[address] = [
            int(timestamp),
            int(tokens[size_index]), tokens[size_index + 1:]
        ]
      elif operation == 'f':
        if address == '0x0000000000000000':
          continue
        if address in allocations:
          del allocations[address]
        else:
          print('freeing untracked address', address, 'on line', tokens[0],
                SymbolizeStack(symbols, tokens[size_index + 1:]))
      else:
        print('invalid operation', operation, 'on line', tokens[0], line_no)

  LogAllocations(symbols, allocations)


AnalyzeFile('alloc.log', 'alloc_symbols.log')
