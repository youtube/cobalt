#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
"""Convert binary files to source arrays.

This script takes all of the given input files and directories (and all
subdirectories) and concatenates them for access at byte arrays in a c++ header
file.

The utility function GenerateMap() is also generated to populate a std::map that
maps between filenames and the embedded file.

All generated symbols will be placed under the user-provided namespace.
"""

import os
import sys


class InputFile(object):
  """A file with an absolute path and a path that it is relative to."""

  def __init__(self, path, parent):
    self.path = os.path.abspath(path)
    self.parent = os.path.abspath(parent)

  def GetRelativePath(self):
    return os.path.relpath(self.path, self.parent)

  def GetVariableName(self):
    # Get rid of non-variable name friendly characters
    return self.GetRelativePath().replace('.', '_').replace('/', '_')


def WriteFileDataToHeader(filename, output_file):
  """Concatenates a single file into the output file."""
  with file(filename, 'rb') as f:
    file_contents = f.read()

    def Chunks(l, n):
      """Yield successive n-sized chunks from l."""
      for i in xrange(0, len(l), n):
        yield l[i:i + n]

    output_string = ',\n'.join([
        ', '.join(['0x%02x' % ord(y) for y in x])
        for x in Chunks(file_contents, 13)
    ])
    output_file.write('{\n' + output_string + '\n};\n\n')


def GetInputFilesToConcatenate(paths):
  """Converts a list of paths into a flattened list of InputFiles.

  If an input path is a directory, it adds an entry for each file in that
  subtree, and keeps it relative to the specified directory.

  If an input path is a regular file, it adds an entry for that file, relative
  to its immediate parent directory.

  Args:
    paths: Array of paths to search for files.

  Returns:
    A list of InputFiles that we would like to concatenate.
  """

  file_list = []
  for path in paths:
    path = os.path.abspath(path)
    if os.path.isdir(path):
      for directory, _, files in os.walk(path):
        for filename in files:
          file_list.append(InputFile(os.path.join(directory, filename), path))
    elif os.path.isfile(path):
      file_list.append(InputFile(path, os.path.dirname(path)))
    else:
      raise '%s is not a file or directory.' % path
  return file_list


def WriteAllFilesToHeader(input_files, output_file):
  """Writes the content of all passed in InputFiles to the header file."""

  for input_file in input_files:
    input_file_variable_name = input_file.GetVariableName()
    output_file.write('const unsigned char %s[] =\n' % input_file_variable_name)
    WriteFileDataToHeader(input_file.path, output_file)


def GenerateMapFunction(input_files, output_file):
  """Generate C++ containing a map.

  Generates a c++ function that populates a map (mapping filenames to the
  embedded contents.)

  Args:
    input_files: List of InputFiles in the map.
    output_file: Name of c++ file to write out.
  """

  # create struct and typedef used in function
  output_file.write(
      'inline void GenerateMap(GeneratedResourceMap &out_map) {\n')

  # os.walk gets dirpath, dirnames, filenames; we want just filenames, so [2]
  for input_file in input_files:
    # The lookup key will be the file path relative to the input directory
    input_file_variable_name = input_file.GetVariableName()
    output_file.write('  out_map["%s"] = FileContents(%s, sizeof(%s));\n' %
                      (input_file.GetRelativePath(), input_file_variable_name,
                       input_file_variable_name))

  output_file.write('}\n\n')


def WriteHeader(namespace, output_file_name, files_to_concatenate):
  """Writes an embedded resource header to the given output filename."""
  output_file = open(output_file_name, 'w')
  include_guard = '_COBALT_GENERATED_' + namespace.upper() + '_H_'
  output_file.write('// Copyright 2014 Google Inc. '
                    'All Rights Reserved.\n'
                    '// This file is generated. Do not edit!\n'
                    '#ifndef ' + include_guard + '\n'
                    '#define ' + include_guard + '\n\n'
                    '#include \"cobalt/base/generated_resources_types.h\"\n\n'
                    'namespace ' + namespace + ' {\n')

  WriteAllFilesToHeader(files_to_concatenate, output_file)
  GenerateMapFunction(files_to_concatenate, output_file)

  output_file.write('} // namespace ' + namespace + '\n\n'
                    '#endif // ' + include_guard + '\n')


def main(namespace, output_file_name, paths):
  WriteHeader(namespace, output_file_name, GetInputFilesToConcatenate(paths))


if __name__ == '__main__':
  if len(sys.argv) < 4:
    print 'usage:\n %s <namespace> <output-file> <inputs...> \n' % sys.argv[0]
    print __doc__
    sys.exit(1)
  main(sys.argv[1], sys.argv[2], sys.argv[3:])
