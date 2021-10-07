#!/usr/bin/python
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""Builds a symlink farm pointing to specified subdirs of the input dir."""

import argparse
import logging
import os
import shutil
import sys

import _env  # pylint: disable=unused-import
from starboard.tools import port_symlink
from starboard.tools import log_level

# The name of an environment variable that when set to |'1'|, signals to us that
# we should log all output directories that we have populated.
_SHOULD_LOG_ENV_KEY = 'STARBOARD_GYP_SHOULD_LOG_COPIES'


def EscapePath(path):
  """Returns a path with spaces escaped."""
  return path.replace(' ', '\\ ')


def _ClearDir(path):
  path = os.path.normpath(path)
  if not os.path.exists(path):  # Works for symlinks for both *nix and Windows.
    return
  port_symlink.Rmtree(path)


def _CheckDepth(max_depth, content_dir):
  """Check that the number of content path components doesn't exceed max_depth.

  The depth is counted as each path component from the content directory itself
  to the deepest file, inclusive.
  """
  # Tuple (depth, path) of a file that's deepest in content_dir.
  walk_deepest = max(
      (root.count(os.sep) + 1, os.path.join(root, (files + dirs)[0]))
      for root, dirs, files in os.walk(content_dir, followlinks=True))
  # Relative path of the deepest file, including the content directory itself.
  deepest_file = os.path.relpath(walk_deepest[1],
                                 os.path.join(content_dir, os.pardir))
  depth = deepest_file.count(os.sep) + 1  # +1 for fencepost error
  logging.info('depth %d: %s', depth, deepest_file)
  if depth > max_depth:
    raise RuntimeError('Content is %d levels deep (max allowed is %d): %s' %
                       (depth, max_depth, deepest_file))


def _CopyTree(src_path, dst_path):
  """Copy tree with a safeguard for windows long path (>260).

  On Windows Python is facing long path limitation, for more details see
  https://bugs.python.org/issue27730
  """
  if os.sep == '\\':
    prefix = '\\\\?\\'
    if prefix not in src_path:
      src_path = prefix + src_path
    if prefix not in dst_path:
      dst_path = prefix + dst_path
  shutil.copytree(src_path, dst_path)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-i', dest='input_dir', required=True, help='input directory')
  parser.add_argument(
      '-o', dest='output_dir', required=True, help='output directory')
  parser.add_argument(
      '-s',
      dest='stamp_file',
      required=True,
      help='stamp file to update after the output directory is populated')
  parser.add_argument(
      '--max_depth',
      type=int,
      help='maximum depth of directories allowed. Depth is not checked if '
      'unspecified or 0. E.g. if output_dir is "content/" then '
      '"content/web/foo/file.txt" is depth of 4.')
  parser.add_argument(
      '--use_absolute_symlinks',
      action='store_true',
      help='Generated symlinks are stored as absolute paths.')
  parser.add_argument(
      'subdirs',
      metavar='subdirs',
      nargs='*',
      help='subdirectories within both the input and output directories')
  parser.add_argument(
      '--copy_override',
      action='store_true',
      help='Overrides the behavior of collect_deploy_content to copy files, '
      'instead of symlinking them.')
  options = parser.parse_args(argv[1:])

  if os.environ.get(_SHOULD_LOG_ENV_KEY, None) == '1':
    log_level.InitializeLoggingWithLevel(logging.INFO)
  else:
    log_level.InitializeLoggingWithLevel(logging.WARNING)

  logging.info('max_depth: %s', options.max_depth)
  logging.info('< %s', options.input_dir)
  logging.info('> %s', options.output_dir)
  for subdir in options.subdirs:
    logging.info('+ %s', subdir)

  if os.path.isdir(options.output_dir):
    _ClearDir(options.output_dir)

  last_link = None
  for subdir in sorted(options.subdirs):
    src_path = os.path.abspath(
        EscapePath(os.path.join(options.input_dir, subdir)))
    dst_path = os.path.abspath(
        EscapePath(os.path.join(options.output_dir, subdir)))

    dst_dir = os.path.dirname(dst_path)
    rel_path = os.path.relpath(src_path, dst_dir)

    # We process subdirs in sorted order so that if there are nested deploy
    # directories we only create the parent and skip all redundant descendants.
    if last_link and src_path.startswith(last_link):
      logging.warning('Redundant deploy content: %s', subdir)
      continue
    last_link = src_path

    logging.info('%s => %s', dst_path, rel_path)

    if not os.path.exists(dst_dir):
      try:
        os.makedirs(dst_dir)
      except Exception as err:  # pylint: disable=broad-except
        msg = 'Error: ' + str(err)
        if os.path.isdir(dst_dir):
          msg += ' path is a directory'
        elif os.path.isfile(dst_dir):
          msg += ' path is a file'
        else:
          msg += ' path points to an unknown type'
        logging.error(msg)

    if options.copy_override:
      _CopyTree(src_path, dst_path)
    elif options.use_absolute_symlinks:
      port_symlink.MakeSymLink(
          target_path=os.path.abspath(src_path),
          link_path=os.path.abspath(dst_path))
    else:
      port_symlink.MakeSymLink(target_path=rel_path, link_path=dst_path)

  if options.max_depth:
    _CheckDepth(options.max_depth, options.output_dir)

  if options.stamp_file:
    with open(options.stamp_file, 'w') as stamp_file:
      stamp_file.write('\n'.join(options.subdirs))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
