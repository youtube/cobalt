#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints paths between gyp targets.
"""

import json
import os
import sys
import time

from collections import deque

def usage():
  print """\
Usage:
  tools/gyp-explain.py chrome_dll gtest#
"""


def GetPath(graph, fro, to):
  """Given a graph in (node -> list of successor nodes) dictionary format,
  yields all paths from |fro| to |to|, starting with the shortest."""
  # Storing full paths in the queue is a bit wasteful, but good enough for this.
  q = deque([(fro, [])])
  while q:
    t, path = q.popleft()
    if t == to:
      yield path + [t]
    for d in graph[t]:
      q.append((d, path + [t]))


def MatchNode(graph, substring):
  """Given a dictionary, returns the key that matches |substring| best. Exits
  if there's not one single best match."""
  candidates = []
  for target in graph:
    if substring in target:
      candidates.append(target)

  if not candidates:
    print 'No targets match "%s"' % substring
    sys.exit(1)
  if len(candidates) > 1:
    print 'More than one target matches "%s": %s' % (
        substring, ' '.join(candidates))
    sys.exit(1)
  return candidates[0]


def Main(argv):
  # Check that dump.json exists and that it's not too old.
  dump_json_dirty = False
  try:
    st = os.stat('dump.json')
    file_age_s = time.time() - st.st_mtime
    if file_age_s > 2 * 60 * 60:
      print 'dump.json is more than 2 hours old.'
      dump_json_dirty = True
  except OSError:
    print 'dump.json not found.'
    dump_json_dirty = True

  if dump_json_dirty:
    print 'Run'
    print '    GYP_GENERATORS=dump_dependency_json build/gyp_chromium'
    print 'first, then try again.'
    sys.exit(1)

  g = json.load(open('dump.json'))

  if len(argv) != 3:
    usage()
    sys.exit(1)

  fro = MatchNode(g, argv[1])
  to = MatchNode(g, argv[2])

  paths = list(GetPath(g, fro, to))
  if len(paths) > 0:
    print 'These paths lead from %s to %s:' % (fro, to)
    for path in paths:
      print ' -> '.join(path)
  else:
    print 'No paths found from %s to %s.' % (fro, to)


if __name__ == '__main__':
  Main(sys.argv)
