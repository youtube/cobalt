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
"""Tool to parse process smaps

Reads and summarizes /proc/<pid>/smaps
"""
import argparse
from collections import namedtuple, OrderedDict, defaultdict
import itertools
import re
import sys


def parse_smaps_entry(entry_lines):
  """Parses a single smaps entry into a dictionary."""
  data = defaultdict(int)
  for line in entry_lines:
    if ':' in line:
      key, value = line.split(':', 1)
      value = value.strip()
      if 'kB' in value:
        data[key.lower().replace('_',
                                 '')] = int(value.replace('kB', '').strip())
  return data


fields = ('size rss pss shr_clean shr_dirty priv_clean priv_dirty '
          'referenced anonymous anonhuge swap swap_pss').split()
MemDetail = namedtuple('name', fields)


def takeuntil(items, predicate):
  """Collects items from an iterator until a predicate is met."""
  groups = itertools.groupby(items, predicate)
  while True:
    try:
      _, pieces = next(groups)
      list_pieces = list(pieces)
      _, last_pice = next(groups)  # pylint: disable=stop-iteration-return
      yield list_pieces + list(last_pice)
    except StopIteration:
      break


def read_smap(args):
  """Reads and summarizes a smaps file."""
  try:
    with open(args.smaps_file, encoding='utf-8') as file:
      lines = [line.rstrip() for line in file]
  except UnicodeDecodeError:
    print(
        f'Warning: Could not decode {args.smaps_file} with UTF-8. Skipping.',
        file=sys.stderr)
    return {}, {}

  owners = {}
  summary = {}
  namewidth = 40 if args.strip_paths else 50

  display_fields = list(fields)
  if args.no_anonhuge:
    display_fields.remove('anonhuge')
  if args.no_shr_dirty:
    display_fields.remove('shr_dirty')

  # The expected output from smaps is 23 lines for each allocation
  for ls in takeuntil(lines, lambda x: x.startswith('VmFlags:')):
    head = re.split(' +', ls.pop(0))
    # Find the name of the memory region
    key = '<anon>'
    if len(head) > 5:
      key = ' '.join(head[5:])

    if args.strip_paths:
      key = re.sub('.*/', '', key)
    if args.remove_so_versions or args.aggregate_solibs:
      key = re.sub(r'\.so[\.0-9]+', '.so', key)
    if args.aggregate_solibs:
      # Split out C / C++ solibs
      key = re.sub(r'libc-[\.0-9]+\.so', '<glibc>', key)
      key = re.sub(r'libstdc\++\.so', '<libstdc++>', key)
      key = re.sub(r'[@\-\.\w]+\.so', '<dynlibs>', key)
    if args.platform == 'android':
      key = re.sub(r'\[(anon:scudo:.*)\]', r'<\1>', key)
      key = re.sub(r'(/dev/ashmem/.*)', r'<\1>', key)
      key = re.sub(r'(/memfd:jit-cache)', r'<\1>', key)
      key = re.sub(r'\[(anon:.*)\]', r'<\1>', key)
      key = re.sub(r'[@\-\.\w]*\.(hyb|vdex|odex|art|jar|oat)[\]]*$', r'<\g<1>>',
                   key)
      key = re.sub(r'anon:stack_and_tls:[0-9a-zA-Z-_]+', '<stack_and_tls>', key)
      key = re.sub(r'[@\-\.\w]+prop:s0', '<prop>', key)
      key = re.sub(r'[@\-\.\w]*@idmap$', '<idmap>', key)

    data = parse_smaps_entry(ls)
    # Ensure all fields are present, defaulting to 0 if not found.
    d = MemDetail(
        data.get('size', 0), data.get('rss', 0), data.get('pss', 0),
        data.get('sharedclean', 0), data.get('shareddirty', 0),
        data.get('privateclean', 0), data.get('privatedirty', 0),
        data.get('referenced', 0), data.get('anonymous', 0),
        data.get('anonhugepages', 0), data.get('swap', 0),
        data.get('swappss', 0))

    if args.aggregate_zeros and (d.rss == 0) and (d.pss == 0):
      key = '<zero resident>'

    addr_range = head[0].split('-')
    start = int('0x' + addr_range[0], 16)
    end = int('0x' + addr_range[1], 16)
    if (end - start) / 1024 != d.size:
      raise RuntimeError(
          f'Sizes dont match: expected {d.size}, got {(end - start) / 1024}')
    lls = owners.get(key, [])
    lls.append(d)
    owners[key] = lls

  if not owners:
    print(
        'Warning: No memory regions were parsed from the smaps file.',
        file=sys.stderr)
    return

  for k, list_allocs in owners.items():
    summary[k] = MemDetail(*map(sum, zip(*list_allocs)))
  sdict = OrderedDict(
      sorted(
          summary.items(),
          reverse=True,
          key=lambda x: getattr(x[1], args.sortkey)))

  if not sdict:
    print('Warning: No summary data could be generated.', file=sys.stderr)
    return

  tabwidth = 12

  def printrow(first_col, col_values):
    print(
        first_col.rjust(namewidth) + '|' +
        '|'.join([str(x).rjust(tabwidth) for x in col_values]))

  printrow('name', display_fields)
  for k, v in sdict.items():
    vp = k.rjust(namewidth)
    printrow(vp[len(vp) - namewidth:], [getattr(v, x) for x in display_fields])

  totals = MemDetail(*map(sum, zip(*sdict.values())))
  printrow('total', [getattr(totals, x) for x in display_fields])
  printrow('name', display_fields)


def get_analysis_parser():
  """Creates and returns the argument parser for smaps analysis."""
  parser = argparse.ArgumentParser(add_help=False)
  parser.add_argument(
      '-k',
      '--sortkey',
      choices=['size', 'rss', 'pss', 'anonymous', 'name'],
      default='pss')
  parser.add_argument(
      '-s',
      '--strip_paths',
      action='store_true',
      help='Remove leading paths from binaries')
  parser.add_argument(
      '-r',
      '--remove_so_versions',
      action='store_true',
      help='Remove dynamic library versions')
  parser.add_argument(
      '-a',
      '--aggregate_solibs',
      action='store_true',
      help='Collapse solibs into single row')
  parser.add_argument(
      '-d',
      '--aggregate_android',
      action='store_true',
      help='Consolidate various Android allocations')
  parser.add_argument(
      '-z',
      '--aggregate_zeros',
      action='store_true',
      help='Consolidate rows that show zero RSS and PSS')
  parser.add_argument(
      '--no_anonhuge',
      action='store_true',
      help='Omit AnonHugePages column from output')
  parser.add_argument(
      '--no_shr_dirty',
      action='store_true',
      help='Omit Shared_Dirty column from output')
  parser.add_argument(
      '--platform',
      choices=['android', 'linux'],
      default='android',
      help='Specify the platform for platform-specific aggregations.')
  return parser


if __name__ == '__main__':
  main_parser = argparse.ArgumentParser(
      description=('A tool to read process memory maps from /proc/<pid>/smaps'
                   ' in a concise way'),
      parents=[get_analysis_parser()])
  main_parser.add_argument(
      'smaps_file',
      help='Contents of /proc/pid/smaps, for example /proc/self/smaps')
  read_smap(main_parser.parse_args())
