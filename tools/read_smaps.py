"""Tool to parse process smaps

Reads and summarizes /proc/<pid>/smaps
"""
import argparse
import re
import itertools
from collections import namedtuple, OrderedDict


def consume_until(l, expect):
  while not l[0].startswith(expect):
    l.pop(0)
  return l.pop(0)


def split_kb_line(in_list, expect):
  l = consume_until(in_list, expect)
  sz = re.split(' +', l)
  if not sz[0].startswith(expect):
    raise RuntimeError(
        f'Unexpexcted line head, looked for:{expect} got:{sz[0]}')
  if sz[2] != 'kB':
    raise RuntimeError(f'Expected kB got {sz[2]}')
  return int(sz[1])


def line_expect(in_list, expect, howmuch):
  val = split_kb_line(in_list, expect)
  if howmuch != val:
    raise RuntimeError(f'Expected {expect} to be {howmuch}, got {val}')
  return 0


fields = ('size rss pss shr_clean shr_dirty priv_clean priv_dirty '
          'referenced anonymous anonhuge').split()
MemDetail = namedtuple('name', fields)


def takeuntil(items, predicate):
  groups = itertools.groupby(items, predicate)
  while True:
    try:
      _, pieces = next(groups)
    except StopIteration:
      return
    list_pieces = list(pieces)
    _, last_pice = list(next(groups))  # pylint: disable=stop-iteration-return
    yield list_pieces + list(last_pice)


def read_smap(args):
  with open(args.smaps_file, encoding='utf-8') as file:
    lines = [line.rstrip() for line in file]
  owners = {}
  summary = {}
  namewidth = 40 if args.strip_paths else 50
  # The expected output from smaps is 23 lines for each allocation
  for ls in takeuntil(lines, lambda x: x.startswith('VmFlags:')):
    head = re.split(' +', ls.pop(0))
    key = (head[5:] or ['<anon>'])[0]
    if args.strip_paths:
      key = re.sub('.*/', '', key)
    if args.remove_so_versions or args.aggregate_solibs:
      key = re.sub(r'\.so[\.0-9]+', '.so', key)
    if args.aggregate_solibs:
      # Split out C / C++ solibs
      key = re.sub(r'libc-[\.0-9]+\.so', '<glibc>', key)
      key = re.sub(r'libstdc\++\.so', '<libstdc++>', key)
      key = re.sub(r'[@\-\.\w]+\.so', '<dynlibs>', key)
    if args.aggregate_android:
      key = re.sub(r'[@\-\.\w]*\.(hyb|vdex|odex|art|jar|oat)[\]]*$', r'<\g<1>>',
                   key)
      key = re.sub(r'anon:stack_and_tls:[0-9a-zA-Z-_]+', '<stack_and_tls>', key)
      key = re.sub(r'[@\-\.\w]+prop:s0', '<prop>', key)
      key = re.sub(r'[@\-\.\w]*@idmap$', '<idmap>', key)
    d = MemDetail(
        split_kb_line(ls, 'Size:') + line_expect(ls, 'KernelPageSize:', 4) +
        line_expect(ls, 'MMUPageSize:', 4), split_kb_line(ls, 'Rss:'),
        split_kb_line(ls, 'Pss:'), split_kb_line(ls, 'Shared_Clean:'),
        split_kb_line(ls, 'Shared_Dirty:'), split_kb_line(ls, 'Private_Clean:'),
        split_kb_line(ls, 'Private_Dirty:'), split_kb_line(ls, 'Referenced:'),
        split_kb_line(ls, 'Anonymous:'), split_kb_line(ls, 'AnonHugePages:'))
    # expected to be constant
    line_expect(ls, 'ShmemPmdMapped:', 0)
    line_expect(ls, 'Shared_Hugetlb:', 0)
    line_expect(ls, 'Private_Hugetlb:', 0)
    if args.aggregate_zeros and (d.rss == 0) and (d.pss == 0):
      key = '<zero resident>'

    addr_range = head[0].split('-')
    start = int('0x' + addr_range[0], 16)
    end = int('0x' + addr_range[1], 16)
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
  read_smap(parser.parse_args())
