#!/usr/bin/env python

# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Returns optional test targets for use in GYP or Python."""

import _env  # pylint: disable=unused-import

import argparse
import os
import posixpath
import sys

from starboard.tools import paths


def _Posixify(path):
  """Returns a normalized, POSIX-ified version of |path|.

  When providing dependencies in GYP the paths must be POSIX paths. This
  function normalizes |path|, and replaces the path separators with the POSIX
  path separator.

  Args:
    path: The path to turn into a normalized, POSIX path.

  Returns:
    A normalized, POSIX path.
  """
  return os.path.normpath(path).replace(os.path.sep, posixpath.sep)


def GetOptionalTestTargets(as_gyp_dependencies=False, depth=''):
  """Returns optional test targets for use in GYP or Python.

  This function will try to import optional test targets, and fail gracefully if
  they cannot be imported.

  When the test targets should be returned in a GYP-friendly format we need to
  ensure that the paths are POSIX paths, and that they are concatenated into a
  single string, delimited by newlines.

  Args:
    as_gyp_dependencies: Whether or not the test targets should be returned in a
      GYP-friendly format.
    depth: The <(DEPTH) of GYP, i.e. the path to prepend to each test target
      when |as_gyp_dependencies| is |True|.

  Returns:
    When |as_gyp_dependencies| is |True|, a string of concatenated paths to test
    targets, otherwise a list of test targets. On failure, either an empty
    string or an empty list.
  """
  try:
    # pylint: disable=g-import-not-at-top
    from starboard.optional.internal import test_targets
    if as_gyp_dependencies:
      return '\n'.join([
          _Posixify(os.path.join(depth, path))
          for path in test_targets.TEST_TARGET_GYP_DEPENDENCIES
      ])
    return test_targets.TEST_TARGET_NAMES
  except ImportError:
    pass
  return '' if as_gyp_dependencies else []


def DoMain(argv=None):
  """Function to allow the use of this script from GYP's pymod_do_main."""
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '-g',
      '--as-gyp-dependencies',
      action='store_true',
      default=False,
      help='Whether or not to return the test targets in a GYP-friendly format.'
  )
  arg_parser.add_argument(
      '-d',
      '--depth',
      default='',
      help='The <(DEPTH) of GYP, i.e. the path to prepend to each test target.')
  args, _ = arg_parser.parse_known_args(argv)

  if args.as_gyp_dependencies != bool(args.depth):
    arg_parser.error('You must specify both --as-gyp-dependencies and --depth.')
  return GetOptionalTestTargets(args.as_gyp_dependencies, args.depth)


def main():
  print(DoMain())
  return 0


if __name__ == '__main__':
  sys.exit(main())
