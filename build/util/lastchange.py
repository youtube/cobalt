#!/usr/bin/env python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
lastchange.py -- Chromium revision fetching utility.
"""

import optparse
import os
import subprocess
import sys

class VersionInfo(object):
  def __init__(self, url, root, revision):
    self.url = url
    self.root = root
    self.revision = revision


def FetchSVNRevision(command, directory):
  """
  Fetch the Subversion branch and revision for the a given directory
  by running the given command (e.g. "svn info").

  Errors are swallowed.

  Returns:
    a VersionInfo object or None on error.
  """

  # Force shell usage under cygwin & win32. This is a workaround for
  # mysterious loss of cwd while invoking cygwin's git.
  # We can't just pass shell=True to Popen, as under win32 this will
  # cause CMD to be used, while we explicitly want a cygwin shell.
  if sys.platform in ('cygwin', 'win32'):
    command = [ 'sh', '-c', ' '.join(command) ];
  try:
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            cwd=directory)
  except OSError:
    # command is apparently either not installed or not executable.
    return None
  if not proc:
    return None

  attrs = {}
  for line in proc.stdout:
    line = line.strip()
    if not line:
      continue
    key, val = line.split(': ', 1)
    attrs[key] = val

  try:
    url = attrs['URL']
    root = attrs['Repository Root']
    revision = attrs['Revision']
  except KeyError:
    return None

  return VersionInfo(url, root, revision)


def FetchVersionInfo(default_lastchange, directory=None):
  """
  Returns the last change (in the form of a branch, revision tuple),
  from some appropriate revision control system.
  """
  version_info = FetchSVNRevision(['svn', 'info'], directory)
  if not version_info:
    version_info = FetchSVNRevision(['git', 'svn', 'info'], directory)
  if not version_info:
    if default_lastchange and os.path.exists(default_lastchange):
      revision = open(default_lastchange, 'r').read().strip()
      version_info = VersionInfo(None, None, revision)
    else:
      version_info = VersionInfo('', '', '0')
  return version_info


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

  version_info = FetchVersionInfo(opts.default_lastchange)

  if opts.revision_only:
    print version_info.revision
  else:
    contents = "LASTCHANGE=%s\n" % version_info.revision
    if out_file:
      WriteIfChanged(out_file, contents)
    else:
      sys.stdout.write(contents)

  return 0


if __name__ == '__main__':
  sys.exit(main())
