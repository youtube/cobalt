#!/usr/bin/python
# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

# This file is based on build/copy_test_data_ios.py
"""Copies data files or directories into a given output directory.

Since the output of this script is intended to be use by GYP, all resulting
paths are using Unix-style forward flashes.
"""

import argparse
import logging
import os
import posixpath
import shutil
import sys
if os.name == 'nt':
  import pywintypes
  import win32api

# The name of an environment variable that when set to |'1'|, signals to us
# that we should log all output directories that we have staged a copy to to
# our stderr output. This output could then, for example, be captured and
# analyzed by an external tool that is interested in these directories.
_ShouldLogEnvKey = 'STARBOARD_GYP_SHOULD_LOG_COPIES'


class WrongNumberOfArgumentsException(Exception):
  pass


def EscapePath(path):
  """Returns a path with spaces escaped."""
  return path.replace(' ', '\\ ')


def ListFilesForPath(path):
  """Returns a list of all the files under a given path."""
  output = []
  # Ignore revision control metadata directories.
  if (os.path.basename(path).startswith('.git') or
      os.path.basename(path).startswith('.svn')):
    return output

  # Files get returned without modification.
  if not os.path.isdir(path):
    output.append(path)
    return output

  # Directories get recursively expanded.
  contents = os.listdir(path)
  for item in contents:
    full_path = posixpath.join(path, item)
    output.extend(ListFilesForPath(full_path))
  return output


def CalcInputs(inputs):
  """Computes the full list of input files. The output is a list of
  (filepath, filepath relative to input dir)"""
  # |inputs| is a list of paths, which may be directories.
  output = []
  for input_file in inputs:
    file_list = ListFilesForPath(input_file)
    dirname = posixpath.dirname(input_file)
    output.extend([(x, posixpath.relpath(x, dirname)) for x in file_list])
  return output


def CopyFiles(files_to_copy, output_basedir):
  """Copies files to the given output directory."""
  for (filename, relative_filename) in files_to_copy:
    if os.name == 'nt':
      # Some of the files (especially for layout tests) result in very long
      # paths (especially on build machines) such that shutil.copy fails.
      try:
        filename = win32api.GetShortPathName(filename)
        output_basedir = win32api.GetShortPathName(output_basedir)
      except pywintypes.error:
        logging.error('Failed to get ShortPathName for filename: \"%s\" and output_basedir: \"%s\"', filename, output_basedir)
        raise
    # In certain cases, files would fail to open on windows if relative paths
    # were provided.  Using absolute paths fixes this.
    filename = os.path.abspath(filename)
    output_basedir = os.path.abspath(output_basedir)
    output_filename = os.path.abspath(os.path.join(output_basedir,
                                                   relative_filename))
    output_dir = os.path.dirname(output_filename)

    # In cases where a directory has turned into a file or vice versa, delete it
    # before copying it below.
    if os.path.exists(output_dir) and not os.path.isdir(output_dir):
      os.remove(output_dir)
    if os.path.exists(output_filename) and os.path.isdir(output_filename):
      shutil.rmtree(output_filename)

    if not os.path.exists(output_dir):
      os.makedirs(output_dir)

    shutil.copy(filename, output_filename)


def DoMain(argv):
  """Called by GYP using pymod_do_main."""
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', dest='output_dir', help='output directory')
  parser.add_argument(
      '--inputs',
      action='store_true',
      dest='list_inputs',
      help='prints a list of all input files')
  parser.add_argument(
      '--outputs',
      action='store_true',
      dest='list_outputs',
      help='prints a list of all output files')
  parser.add_argument(
      'input_paths',
      metavar='path',
      nargs='+',
      help='path to an input file or directory')
  options = parser.parse_args(argv)

  if os.environ.get(_ShouldLogEnvKey, None) == '1':
    if options.output_dir is not None:
      print >> sys.stderr, 'COPY_LOG:', options.output_dir

  files_to_copy = CalcInputs(options.input_paths)
  if options.list_inputs:
    return '\n'.join([EscapePath(x[0]) for x in files_to_copy])

  if not options.output_dir:
    raise WrongNumberOfArgumentsException('-o required.')

  if options.list_outputs:
    outputs = [posixpath.join(options.output_dir, x[1]) for x in files_to_copy]
    return '\n'.join(outputs)

  CopyFiles(files_to_copy, options.output_dir)
  return


def main(argv):
  logging_format = '[%(levelname)s:%(filename)s:%(lineno)s] %(message)s'
  logging.basicConfig(
      level=logging.INFO, format=logging_format, datefmt='%H:%M:%S')

  try:
    result = DoMain(argv[1:])
  except WrongNumberOfArgumentsException, e:
    print >> sys.stderr, e
    return 1
  if result:
    print result
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
