#!/usr/bin/env python
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Modifies a list of input paths to produce a list of output paths.

The user can specify a list of paths as well as specific path transformations
(e.g. replace directory or replace extension) and this script will return a
list of processed output paths.
"""

import argparse
import os
import sys
import textwrap


def ConvertPath(path,
                output_directory=None,
                output_prefix=None,
                output_extension=None,
                forward_slashes=None,
                base_directory=None):
  """The script's core function."""
  output_path = path

  if output_directory:
    relpath_start = base_directory
    if not relpath_start:
      relpath_start = os.path.dirname(output_path)
    relpath = os.path.relpath(output_path, relpath_start)
    assert os.pardir not in relpath
    output_path = os.path.join(output_directory, relpath)

  if output_prefix:
    output_path = os.path.join(
        os.path.dirname(output_path),
        output_prefix + os.path.basename(output_path))
  if output_extension:
    output_path = (
        os.path.splitext(output_path)[0] + os.extsep + output_extension)
  if forward_slashes:
    output_path = output_path.replace('\\', '/')

  return output_path


def DoMain(argv):
  """Script main function."""
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description=textwrap.dedent(__doc__))
  parser.add_argument(
      '-d',
      '--output_directory',
      type=str,
      help=('If specified, will replace all input path '
            'directories with the one specified by this '
            'parameter.'))
  parser.add_argument(
      '-e',
      '--output_extension',
      type=str,
      help=('If specified, will replace all input path '
            'extensions with the one specified by this '
            'parameter.'))
  parser.add_argument(
      '-p',
      '--output_prefix',
      type=str,
      help=('If specified, will prepend the string '
            'specified in the parameter to the filename.'))
  parser.add_argument(
      '-s',
      '--forward_slashes',
      action='store_true',
      help=('Set this flag if you would like all output '
            'paths to contain forward slashes, even on '
            'Windows.  This is useful if you are calling this '
            'from Gyp which expects forward slashes. Used with '
            'the -d argument.'))
  parser.add_argument(
      '-b',
      '--base_directory',
      type=str,
      help=('If specified, remove up to and including the '
            'specified directory from the input path when '
            'constructing the output path. The input paths '
            'must be under the base directory. The -d argument '
            'must also be specified.'))
  parser.add_argument(
      'input_paths',
      help=('A list of paths that will be modified as '
            'specified by the other options.'),
      nargs='*')
  args = parser.parse_args(argv)

  converted_paths = [
      ConvertPath(path, args.output_directory, args.output_prefix,
                  args.output_extension, args.forward_slashes,
                  args.base_directory) for path in args.input_paths
  ]
  return ' '.join(converted_paths)


if __name__ == '__main__':
  print DoMain(sys.argv[1:])
  sys.exit(0)
