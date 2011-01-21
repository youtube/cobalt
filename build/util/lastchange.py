#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
lastchange.py -- Chromium revision fetching utility.
"""

import optparse
import os
import re
import subprocess
import sys


def FetchSVNRevision(command):
  """
  Fetch the Subversion revision for the local tree.

  Errors are swallowed.
  """
  try:
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            shell=(sys.platform=='win32'))
  except OSError:
    # command is apparently either not installed or not executable.
    return None
  if proc:
    svn_re = re.compile('^Revision:\s+(\d+)', re.M)
    match = svn_re.search(proc.stdout.read())
    if match:
      return match.group(1)
  return None


def FetchChange(default_lastchange):
  """
  Returns the last change, from some appropriate revision control system.
  """
  change = FetchSVNRevision(['svn', 'info'])
  if not change and sys.platform in ('linux2',):
    change = FetchSVNRevision(['git', 'svn', 'info'])
  if not change:
    if default_lastchange and os.path.exists(default_lastchange):
      change = open(default_lastchange, 'r').read().strip()
    else:
      change = '0'
  return change


def WriteIfChanged(file_name, contents):
  """
  Writes the specified contents to the specified file_name
  iff the contents are different than the current contents.
  """
  try:
    old_contents = open(file_name, 'r').read()
  except EnvironmentError:
    pass
  else:
    if contents == old_contents:
      return
    os.unlink(file_name)
  open(file_name, 'w').write(contents)


def main(argv=None):
  if argv is None:
    argv = sys.argv

  parser = optparse.OptionParser(usage="lastchange.py [options]")
  parser.add_option("-d", "--default-lastchange", metavar="FILE",
                    help="default last change input FILE")
  parser.add_option("-o", "--output", metavar="FILE",
                    help="write last change to FILE")
  parser.add_option("--revision-only", action='store_true',
                    help="just print the SVN revision number")
  opts, args = parser.parse_args(argv[1:])

  out_file = opts.output

  while len(args) and out_file is None:
    if out_file is None:
      out_file = args.pop(0)
  if args:
    sys.stderr.write('Unexpected arguments: %r\n\n' % args)
    parser.print_help()
    sys.exit(2)

  change = FetchChange(opts.default_lastchange)

  if opts.revision_only:
    print change
  else:
    contents = "LASTCHANGE=%s\n" % change
    if out_file:
      WriteIfChanged(out_file, contents)
    else:
      sys.stdout.write(contents)

  return 0


if __name__ == '__main__':
  sys.exit(main())
