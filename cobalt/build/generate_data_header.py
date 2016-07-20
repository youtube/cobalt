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

This script takes all of the files in a given directory (and all of its
subdirectories) and concatenates them for access at byte arrays in a c++
header file.

The directory to check is specified as the first argument, and the output
header is specified as the second argument.

The utility function GenerateMap() is also generated to populate a std::map
that maps between filenames and the embedded file.

All generated symbols will be placed under the user-provided namespace.
"""

import os
import sys


def GetVariableNameFromPath(file_path):
  # Get rid of non-variable name friendly characters
  return file_path.replace('.', '_').replace('/', '_')


def WriteFileDataToHeader(input_file, output_file):
  """Concatenates a single file into the output file."""
  with file(input_file, 'rb') as f:
    file_contents = f.read()
    def chunks(l, n):
      """ Yield successive n-sized chunks from l."""
      for i in xrange(0, len(l), n):
        yield l[i:i+n]

    output_string = ',\n'.join([', '.join(['0x%02x' % ord(y) for y in x])
                    for x in chunks(file_contents, 200)])
    output_file.write('{' + output_string + '};\n\n')


def GetFilesToConcatenate(input_directory):
  """Get list of files to concatenate.

  Args:
    input_directory: Directory to search for files.
  Returns:
    A list of all files that we would like to concatenate relative
    to the input directory.

  """

  file_list = []
  for dirpath, _, files in os.walk(input_directory):
    for input_file in files:
      file_list.append(
          os.path.relpath(
              os.path.join(dirpath, input_file),
              input_directory))
  return file_list


def WriteAllFilesToHeader(input_directory, files, output_file):
  """Writes the content of all passed in files to the header file."""

  for path in files:
    input_file_variable_name = GetVariableNameFromPath(path)
    output_file.write('const unsigned char %s[] =\n' % input_file_variable_name)

    WriteFileDataToHeader(os.path.join(input_directory, path), output_file)


def GenerateMapFunction(files, output_file):
  """Generate C++ containing a map.

  Generates a c++ function that populates a map (mapping filenames to the
  embedded contents.)

  Args:
    files: List of filenames in the map.
    output_file: Name of c++ file to write out.
  """

  # create struct and typedef used in function
  output_file.write(
      'inline void GenerateMap(GeneratedResourceMap &out_map) {\n')

  # os.walk gets dirpath, dirnames, filenames; we want just filenames, so [2]
  for path in files:
    # The lookup key will be the file path relative to the input directory
    input_file_variable_name = GetVariableNameFromPath(path)
    output_file.write('  out_map["%s"] = FileContents(%s, sizeof(%s));\n' %
                      (path, input_file_variable_name, input_file_variable_name)
                     )

  output_file.write('}\n\n')


def main(namespace, input_directory, output_file_name):
  output_file = open(output_file_name, 'w')
  include_guard = '_COBALT_GENERATED_' + namespace.upper() + '_H_'
  output_file.write('// Copyright 2014 Google Inc. '
                    'All Rights Reserved.\n'
                    '// This file is generated. Do not edit!\n'
                    '#ifndef ' + include_guard + '\n'
                    '#define ' + include_guard + '\n\n'
                    '#include \"cobalt/base/generated_resources_types.h\"\n\n'
                    'namespace ' + namespace +' {\n')

  files_to_concatenate = GetFilesToConcatenate(input_directory)
  WriteAllFilesToHeader(input_directory,
                        files_to_concatenate,
                        output_file)
  GenerateMapFunction(files_to_concatenate, output_file)

  output_file.write('} // namespace ' + namespace + '\n\n'
                    '#endif // ' + include_guard + '\n')


if __name__ == '__main__':
  if len(sys.argv) != 4:
    print ('usage:\n %s <namespace> <input directory> <output filepath>' %
           sys.argv[0])
    print __doc__
    sys.exit(1)
  main(sys.argv[1], sys.argv[2], sys.argv[3])
