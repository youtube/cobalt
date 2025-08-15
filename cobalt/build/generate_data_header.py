#!/usr/bin/python
#
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
  with open(filename, 'rb') as f:
    file_contents = f.read()

    def Chunks(l, n):
      """Yield successive n-sized chunks from l."""
      for i in range(0, len(l), n):
        yield l[i:i + n]

    # TODO(b/154137263): Remove once python2 is gone.
    is_py2 = sys.version_info.major == 2

    def GetByte(x):
      return ord(x) if is_py2 else x

    output_string = ',\n'.join([
        ', '.join([f'0x{GetByte(y):02x}'
                   for y in x])
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
      raise ValueError(f'{path} is not a file or directory.')
  return file_list


def WriteAllFilesToHeader(input_files, output_file):
  """Writes the content of all passed in InputFiles to the header file."""

  for input_file in input_files:
    input_file_variable_name = input_file.GetVariableName()
    output_file.write(f'const unsigned char {input_file_variable_name}[] =\n')
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
    output_file.write(f'  out_map["{input_file.GetRelativePath()}"] = '
                      f'FileContents({input_file_variable_name}, '
                      f'sizeof({input_file_variable_name}));\n')

  output_file.write('}\n\n')


def WriteHeader(namespace, output_file_name, files_to_concatenate):
  """Writes an embedded resource header to the given output filename."""
  with open(output_file_name, 'w', encoding='utf-8') as output_file:
    include_guard = '_COBALT_GENERATED_' + namespace.upper() + '_H_'
    output_file.write('// Copyright 2014 The Cobalt Authors. '
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
    print(f'usage:\n {sys.argv[0]} <namespace> <output-file> <inputs...> \n')
    print(__doc__)
    sys.exit(1)
  main(sys.argv[1], sys.argv[2], sys.argv[3:])
