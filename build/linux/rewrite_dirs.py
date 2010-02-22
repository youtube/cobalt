#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rewrites paths in -I, -L and other option to be relative to a sysroot."""

import sys
import os

REWRITE_PREFIX = ['-I',
                  '-idirafter',
                  '-imacros',
                  '-imultilib',
                  '-include',
                  '-iprefix',
                  '-iquote',
                  '-isystem',
                  '-L']

def RewritePath(path, sysroot):
  """Rewrites a path by prefixing it with the sysroot if it is absolute."""
  if os.path.isabs(path) and not path.startswith(sysroot):
    path = path.lstrip('/')
    return os.path.join(sysroot, path)
  else:
    return path

def RewriteLine(line, sysroot):
  """Rewrites all the paths in recognized options."""
  args = line.split()
  count = len(args)
  i = 0
  while i < count:
    for prefix in REWRITE_PREFIX:
      # The option can be either in the form "-I /path/to/dir" or
      # "-I/path/to/dir" so handle both.
      if args[i] == prefix:
        i += 1
        try:
          args[i] = RewritePath(args[i], sysroot)
        except IndexError:
          sys.stderr.write('Missing argument following %s\n' % prefix)
          break
      elif args[i].startswith(prefix):
        args[i] = prefix + RewritePath(args[i][len(prefix):], sysroot)
    i += 1

  return ' '.join(args)

def main(argv):
  try:
    sysroot = argv[1]
  except IndexError:
    sys.stderr.write('usage: %s /path/to/sysroot\n' % argv[0])
    return 1

  for line in sys.stdin.readlines():
    line = RewriteLine(line.strip(), sysroot)
    print line
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
