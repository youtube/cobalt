#!/usr/bin/python
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

"""Converts XLB files into CSV files in a given output directory.

Since the output of this script is intended to be use by GYP, all resulting
paths are using Unix-style forward slashes.
"""

import argparse
import os
import posixpath
import sys
import xml.etree.ElementTree


class WrongNumberOfArgumentsException(Exception):
  pass


def EscapePath(path):
  """Returns a path with spaces escaped."""
  return path.replace(' ', '\\ ')


def ChangeSuffix(filename, new_suffix):
  """Changes the suffix of |filename| to |new_suffix|. If no current suffix,
  adds |new_suffix| to the end of |filename|."""
  (root, ext) = os.path.splitext(filename)
  return root + '.' + new_suffix


def ConvertSingleFile(filename, output_filename):
  """Converts a single input XLB file to a CSV file."""
  tree = xml.etree.ElementTree.parse(filename)
  root = tree.getroot()

  # First child of the root is the list of messages.
  messages = root[0]

  # Write each message to the output file on its own line.
  with open(output_filename, 'wb') as output_file:
    for msg in messages:
      # Use ; as the separator. Which means it better not be in the name.
      assert not (';' in msg.attrib['name'])
      output_file.write(msg.attrib['name'])
      output_file.write(';')
      # Encode the text as UTF8 to accommodate special characters.
      output_file.write(msg.text.encode('utf8'))
      output_file.write('\n')


def GetOutputs(files_to_convert, output_basedir):
  """Returns a list of filenames relative to the output directory,
  based on a list of input files."""
  outputs = [];
  for filename in files_to_convert:
    dirname = posixpath.dirname(filename)
    relative_filename = posixpath.relpath(filename, dirname)
    relative_filename = ChangeSuffix(relative_filename, 'csv')
    output_filename = posixpath.join(output_basedir, relative_filename)
    outputs.append(output_filename)

  return outputs


def ConvertFiles(files_to_convert, output_basedir):
  """Converts files and writes the result the given output directory."""
  for filename in files_to_convert:
    dirname = posixpath.dirname(filename)
    relative_filename = posixpath.relpath(filename, dirname)
    output_filename = posixpath.join(output_basedir, relative_filename)
    output_dir = posixpath.dirname(output_filename)

    if not os.path.exists(output_dir):
      os.makedirs(output_dir)

    output_filename = ChangeSuffix(output_filename, 'csv')
    print 'Converting ' + filename + ' to ' + output_filename
    ConvertSingleFile(filename, output_filename)


def DoMain(argv):
  """Called by GYP using pymod_do_main."""
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', dest='output_dir', help='output directory')
  parser.add_argument('--inputs', action='store_true', dest='list_inputs',
                      help='prints a list of all input files')
  parser.add_argument('--outputs', action='store_true', dest='list_outputs',
                      help='prints a list of all output files')
  parser.add_argument('input_paths', metavar='path', nargs='+',
                      help='path to an input file or directory')
  options = parser.parse_args(argv)

  files_to_convert = [EscapePath(x) for x in options.input_paths]

  if options.list_inputs:
    return '\n'.join(files_to_convert)

  if not options.output_dir:
    raise WrongNumberOfArgumentsException('-o required.')

  if options.list_outputs:
    outputs = GetOutputs(files_to_convert, options.output_dir)
    return '\n'.join(outputs)

  ConvertFiles(files_to_convert, options.output_dir)
  return


def main(argv):
  print 'Running... in main()'
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
