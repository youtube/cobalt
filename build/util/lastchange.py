#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
lastchange.py -- Chromium revision fetching utility.
"""

import re
import optparse
import os
import subprocess
import sys

class VersionInfo(object):
  def __init__(self, url, root, revision):
    self.url = url
    self.root = root
    self.revision = revision


def FetchSVNRevision(directory):
  """
  Fetch the Subversion branch and revision for a given directory.

  Errors are swallowed.

  Returns:
    a VersionInfo object or None on error.
  """
  try:
    proc = subprocess.Popen(['svn', 'info'],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            cwd=directory,
                            shell=(sys.platform=='win32'))
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


def RunGitCommand(directory, command):
  """
  Launches git subcommand.

  Errors are swallowed.

  Returns:
    process object or None.
  """
  command = ['git'] + command
  # Force shell usage under cygwin & win32. This is a workaround for
  # mysterious loss of cwd while invoking cygwin's git.
  # We can't just pass shell=True to Popen, as under win32 this will
  # cause CMD to be used, while we explicitly want a cygwin shell.
  if sys.platform in ('cygwin', 'win32'):
    command = ['sh', '-c', ' '.join(command)]
  try:
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            cwd=directory)
    return proc
  except OSError:
    return None


def FetchGitRevision(directory):
  """
  Fetch the Git hash for a given directory.

  Errors are swallowed.

  Returns:
    a VersionInfo object or None on error.
  """
  proc = RunGitCommand(directory, ['rev-parse', 'HEAD'])
  if proc:
    output = proc.communicate()[0].strip()
    if proc.returncode == 0 and output:
      return VersionInfo('git', 'git', output[:7])
  return None


def IsGitSVN(directory):
  """
  Checks whether git-svn has been set up.

  Errors are swallowed.

  Returns:
    whether git-svn has been set up.
  """
  # To test whether git-svn has been set up, query the config for any
  # svn-related configuration.  This command exits with an error code
  # if there aren't any matches, so ignore its output.
  proc = RunGitCommand(directory, ['config', '--get-regexp', '^svn'])
  if proc:
    return (proc.wait() == 0)
  return False


def FetchGitSVNURL(directory):
  """
  Fetch URL of SVN repository bound to git.

  Errors are swallowed.

  Returns:
    SVN URL.
  """
  if IsGitSVN(directory):
    proc = RunGitCommand(directory, ['svn', 'info', '--url'])
    if proc:
      output = proc.communicate()[0].strip()
      if proc.returncode == 0:
        match = re.search(r'^\w+://.*$', output, re.M)
        if match:
          return match.group(0)
  return ''


def FetchGitSVNRoot(directory):
  """
  Fetch root of SVN repository bound to git.

  Errors are swallowed.

  Returns:
    SVN root repository.
  """
  if IsGitSVN(directory):
    git_command = ['config', '--get-regexp', '^svn-remote.svn.url$']
    proc = RunGitCommand(directory, git_command)
    if proc:
      output = proc.communicate()[0].strip()
      if proc.returncode == 0:
        # Zero return code implies presence of requested configuration variable.
        # Its value is second (last) field of output.
        match = re.search(r'\S+$', output)
        if match:
          return match.group(0)
  return ''


def LookupGitSVNRevision(directory, depth):
  """
  Fetch the Git-SVN identifier for the local tree.
  Parses first |depth| commit messages.

  Errors are swallowed.
  """
  if not IsGitSVN(directory):
    return None
  git_re = re.compile(r'^\s*git-svn-id:\s+(\S+)@(\d+)')
  proc = RunGitCommand(directory, ['log', '-' + str(depth)])
  if proc:
    for line in proc.stdout:
      match = git_re.match(line)
      if match:
        id = match.group(2)
        if id:
          proc.stdout.close()  # Cut pipe for fast exit.
          return id
  return None


def IsGitSVNDirty(directory):
  """
  Checks whether our git-svn tree contains clean trunk or some branch.

  Errors are swallowed.
  """
  # For git branches the last commit message is either
  # some local commit or a merge.
  return LookupGitSVNRevision(directory, 1) is None


def FetchGitSVNRevision(directory):
  """
  Fetch the Git-SVN identifier for the local tree.

  Errors are swallowed.
  """
  # We assume that at least first 999 commit messages contain svn evidence.
  revision = LookupGitSVNRevision(directory, 999)
  if not revision:
    return None
  if IsGitSVNDirty(directory):
    revision = revision + '-dirty'
  url = FetchGitSVNURL(directory)
  root = FetchGitSVNRoot(directory)
  return VersionInfo(url, root, revision)


def FetchVersionInfo(default_lastchange, directory=None):
  """
  Returns the last change (in the form of a branch, revision tuple),
  from some appropriate revision control system.
  """
  version_info = (FetchSVNRevision(directory) or
      FetchGitSVNRevision(directory) or FetchGitRevision(directory))
  if not version_info:
    if default_lastchange and os.path.exists(default_lastchange):
      revision = open(default_lastchange, 'r').read().strip()
      version_info = VersionInfo(None, None, revision)
    else:
      version_info = VersionInfo('unknown', '', '0')
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
