"""Tool to parse process smaps

Reads and summarizes /proc/<pid>/smaps
"""
import argparse
import re
from itertools import islice
from collections import namedtuple, OrderedDict


def chunks(items, chunk_size):
  iterator = iter(items)
  while True:
    chunk = list(islice(iterator, chunk_size))
    if not chunk:
      break
    yield chunk


def split_kb_line(l, expect):
  sz = re.split(' +', l)
  if not sz[0].startswith(expect):
    raise RuntimeError(
        f'Unexpexcted line head, looked for:{expect} got:{sz[0]}')
  if sz[2] != 'kB':
    raise RuntimeError(f'Expected kB got {sz[2]}')
  return int(sz[1])


def line_expect(value, what, howmuch):
  val = split_kb_line(value, what)
  if howmuch != val:
    raise RuntimeError(f'Expected {what} to be {howmuch}, got {val}')


fields = ('size rss pss shr_clean priv_clean priv_dirty '
          'referenced anonymous').split()
MemDetail = namedtuple('name', fields)


def read_smap(args):
  with open(args.smaps_file, encoding='utf-8') as file:
    lines = [line.rstrip() for line in file]
  owners = {}
  summary = {}
  namewidth = 20 if args.strip_paths else 50
  # The expected output from smaps is 23 lines for each allocation
  for ls in chunks(lines, 23):
    head = re.split(' +', ls[0])
    key = (head[5:] or ['<anon>'])[0]
    if args.strip_paths:
      key = re.sub('.*/', '', key)
    if args.remove_so_versions or args.aggregate_solibs:
      key = re.sub(r'\.so[\.0-9]+', '.so', key)
    if args.aggregate_solibs:
      # Split out C / C++ solibs
      key = re.sub(r'libc-[\.0-9]+\.so', '<glibc>', key)
      key = re.sub(r'libstdc\++\.so', '<libstdc++>', key)
      key = re.sub(r'[0-9a-zA-Z-_\.]+\.so', '<dynlibs>', key)
    d = MemDetail(
        split_kb_line(ls[1], 'Size:'), split_kb_line(ls[4], 'Rss:'),
        split_kb_line(ls[5], 'Pss:'), split_kb_line(ls[6], 'Shared_Clean:'),
        split_kb_line(ls[8], 'Private_Clean:'),
        split_kb_line(ls[9], 'Private_Dirty:'),
        split_kb_line(ls[10], 'Referenced:'),
        split_kb_line(ls[11], 'Anonymous:'))
    # expected to be constant
    line_expect(ls[2], 'KernelPageSize:', 4)
    line_expect(ls[3], 'MMUPageSize:', 4)
    line_expect(ls[7], 'Shared_Dirty:', 0)
    line_expect(ls[12], 'LazyFree:', 0)
    line_expect(ls[13], 'AnonHugePages:', 0)
    line_expect(ls[14], 'ShmemPmdMapped:', 0)
    line_expect(ls[15], 'FilePmdMapped:', 0)
    line_expect(ls[16], 'Shared_Hugetlb:', 0)
    line_expect(ls[17], 'Private_Hugetlb:', 0)
    line_expect(ls[18], 'Swap:', 0)
    line_expect(ls[19], 'SwapPss:', 0)
    addrrange = head[0].split('-')
    start = int('0x' + addrrange[0], 16)
    end = int('0x' + addrrange[1], 16)
    if (end - start) / 1024 != d.size:
      raise RuntimeError('Sizes dont match')
    lls = owners.get(key, [])
    lls.append(d)
    owners[key] = lls

  for k, list_allocs in owners.items():
    summary[k] = MemDetail(*map(sum, zip(*list_allocs)))
  sdict = OrderedDict(
      sorted(
          summary.items(),
          reverse=True,
          key=lambda x: getattr(x[1], args.sortkey)))

  tabwidth = 12

  def printrow(first_col, col_values):
    print(
        first_col.rjust(namewidth) + '|' +
        '|'.join([str(x).rjust(tabwidth) for x in col_values]))

  printrow('name', fields)
  for k, v in sdict.items():
    vp = k.rjust(namewidth)
    printrow(vp[len(vp) - namewidth:], [getattr(v, x) for x in fields])

  totals = MemDetail(*map(sum, zip(*sdict.values())))
  printrow('total', [getattr(totals, x) for x in fields])
  printrow('name', fields)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description=('A tool to read process memory maps from /proc/<pid>/smaps'
                   ' in a concise way'))
  parser.add_argument(
      'smaps_file',
      help='Contents of /proc/pid/smaps, for example /proc/self/smaps')
  parser.add_argument(
      '--sortkey',
      choices=['size', 'rss', 'pss', 'anonymous', 'name'],
      default='pss')
  parser.add_argument(
      '--strip_paths',
      action='store_true',
      help='Remove leading paths from binaries')
  parser.add_argument(
      '--remove_so_versions',
      action='store_true',
      help='Remove dynamic library versions')
  parser.add_argument(
      '--aggregate_solibs',
      action='store_true',
      help='Collapse solibs into single row')
  read_smap(parser.parse_args())
