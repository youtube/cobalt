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

def ConvertPaths(input_paths, output_directory, output_prefix,
                 output_extension, forward_slashes):
  """The script's core function."""
  def ConvertPath(path):
    output_path = path

    if output_directory:
      output_path = os.path.join(output_directory,
                                 os.path.basename(output_path))
    if output_prefix:
      output_path = os.path.join(os.path.dirname(output_path),
                                 output_prefix + os.path.basename(output_path))
    if output_extension:
      output_path = (os.path.splitext(output_path)[0] +
                     os.extsep + output_extension)
    if forward_slashes:
      output_path = output_path.replace('\\', '/')

    return output_path

  # If an element in the list of input paths contains a space, treat that as
  # a separater for more input paths and expand the list of input paths.
  # Gyp does not make it easy to marshall paramaters into a Python script so
  # this functionality is for working around that problem.
  expanded_input_paths = []
  for x in input_paths:
    expanded_input_paths += x.split(' ')

  return [ConvertPath(x) for x in expanded_input_paths]

def DoMain(argv):
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description=textwrap.dedent(__doc__))
  parser.add_argument('-d', '--output_directory',
                      type=str,
                      help=('If specified, will replace all input path '
                            'directories with the one specified by this '
                            'parameter.'))
  parser.add_argument('-e', '--output_extension',
                      type=str,
                      help=('If specified, will replace all input path '
                            'extensions with the one specified by this '
                            'parameter.'))
  parser.add_argument('-p', '--output_prefix',
                      type=str,
                      help=('If specified, will prepend the string '
                            'specified in the parameter to the filename.'))
  parser.add_argument('-s', '--forward_slashes',
                      action='store_true',
                      help=('Set this flag if you would like all output '
                            'paths to contain forward slashes, even on '
                            'Windows.  This is useful if you are calling this '
                            'from Gyp which expects forward slashes.'))
  parser.add_argument('input_paths',
                      help=('A list of paths that will be modified as '
                            'specified by the other options.'),
                      nargs='*')
  args = parser.parse_args(argv)

  return ' '.join(ConvertPaths(args.input_paths,
                               args.output_directory,
                               args.output_prefix,
                               args.output_extension,
                               args.forward_slashes))

if __name__ == '__main__':
  print DoMain(sys.argv[1:])
  sys.exit(0)
