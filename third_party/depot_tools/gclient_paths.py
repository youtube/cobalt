# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is imported by various thin wrappers (around gn, clang-format, ...),
# so it's meant to import very quickly. To keep it that way don't add more
# code, and even more importantly don't add more toplevel import statements,
# particularly for modules that are not builtin (see sys.builtin_modules_names,
# os isn't built in, but it's essential to this file).

from __future__ import print_function

import gclient_utils
import logging
import os
import subprocess2
import sys


def FindGclientRoot(from_dir, filename='.gclient'):
  """Tries to find the gclient root."""
  real_from_dir = os.path.abspath(from_dir)
  path = real_from_dir
  while not os.path.exists(os.path.join(path, filename)):
    split_path = os.path.split(path)
    if not split_path[1]:
      return None
    path = split_path[0]

  logging.info('Found gclient root at ' + path)

  if path == real_from_dir:
    return path

  # If we did not find the file in the current directory, make sure we are in a
  # sub directory that is controlled by this configuration.
  entries_filename = os.path.join(path, filename + '_entries')
  if not os.path.exists(entries_filename):
    # If .gclient_entries does not exist, a previous call to gclient sync
    # might have failed. In that case, we cannot verify that the .gclient
    # is the one we want to use. In order to not to cause too much trouble,
    # just issue a warning and return the path anyway.
    print(
        "%s missing, %s file in parent directory %s might not be the file "
        "you want to use." % (entries_filename, filename, path),
        file=sys.stderr)
    return path

  entries_content = gclient_utils.FileRead(entries_filename)
  scope = {}
  try:
    exec(entries_content, scope)
  except (SyntaxError, Exception) as e:
    gclient_utils.SyntaxErrorToError(filename, e)

  all_directories = scope['entries'].keys()
  path_to_check = os.path.relpath(real_from_dir, path)
  while path_to_check:
    if path_to_check in all_directories:
      return path
    path_to_check = os.path.dirname(path_to_check)

  return None


def GetPrimarySolutionPath():
  """Returns the full path to the primary solution. (gclient_root + src)"""

  gclient_root = FindGclientRoot(os.getcwd())
  if gclient_root:
    # Some projects' top directory is not named 'src'.
    source_dir_name = GetGClientPrimarySolutionName(gclient_root) or 'src'
    return os.path.join(gclient_root, source_dir_name)

  # Some projects might not use .gclient. Try to see whether we're in a git
  # checkout that contains a 'buildtools' subdir.
  top_dir = os.getcwd()
  try:
    top_dir = subprocess2.check_output(['git', 'rev-parse', '--show-toplevel'],
                                       stderr=subprocess2.DEVNULL)
    if sys.version_info.major == 3:
      top_dir = top_dir.decode('utf-8', 'replace')
    top_dir = os.path.normpath(top_dir.strip())
  except subprocess2.CalledProcessError:
    pass

  if os.path.exists(os.path.join(top_dir, 'buildtools')):
    return top_dir
  return None


def GetBuildtoolsPath():
  """Returns the full path to the buildtools directory.
  This is based on the root of the checkout containing the current directory."""

  # Overriding the build tools path by environment is highly unsupported and may
  # break without warning.  Do not rely on this for anything important.
  override = os.environ.get('CHROMIUM_BUILDTOOLS_PATH')
  if override is not None:
    return override

  primary_solution = GetPrimarySolutionPath()
  if not primary_solution:
    return None

  buildtools_path = os.path.join(primary_solution, 'buildtools')
  if os.path.exists(buildtools_path):
    return buildtools_path

  # buildtools may be in the gclient root.
  gclient_root = FindGclientRoot(os.getcwd())
  buildtools_path = os.path.join(gclient_root, 'buildtools')
  if os.path.exists(buildtools_path):
    return buildtools_path

  return None


def GetBuildtoolsPlatformBinaryPath():
  """Returns the full path to the binary directory for the current platform."""
  buildtools_path = GetBuildtoolsPath()
  if not buildtools_path:
    return None

  if sys.platform.startswith(('cygwin', 'win')):
    subdir = 'win'
  elif sys.platform == 'darwin':
    subdir = 'mac'
  elif sys.platform.startswith('linux'):
    subdir = 'linux64'
  else:
    raise gclient_utils.Error('Unknown platform: ' + sys.platform)
  return os.path.join(buildtools_path, subdir)


def GetExeSuffix():
  """Returns '' or '.exe' depending on how executables work on this platform."""
  if sys.platform.startswith(('cygwin', 'win')):
    return '.exe'
  return ''


def GetGClientPrimarySolutionName(gclient_root_dir_path):
  """Returns the name of the primary solution in the .gclient file specified."""
  gclient_config_file = os.path.join(gclient_root_dir_path, '.gclient')
  gclient_config_contents = gclient_utils.FileRead(gclient_config_file)
  env = {}
  exec(gclient_config_contents, env)
  solutions = env.get('solutions', [])
  if solutions:
    return solutions[0].get('name')
  return None
