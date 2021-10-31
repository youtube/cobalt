#!/usr/bin/python
# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Associates GLSL files with platform-specific shaders based on filename.

This script scans all input files and detects whether they are GLSL files or
not, based on file extension (e.g. "*.glsl" files are classified as GLSL
files, everything else is not a GLSL file).  After this, GLSL files that share
the same base filename with a non-GLSL file is then associated with it.

At this point we have a mapping from GLSL files to platform-specific shader
file.  From here, we hash the GLSL file contents to obtain a mapping from GLSL
hash to platform-specific shader file.  Once in this form, the data is ready to
be saved.

The GLSL hash to platform-specific shader data is saved as a C++ header file,
at which point it can be included by C++ code.  The C++ code should call
GenerateGLSLShaderMap() to obtain a std::map<uint32_t, ShaderData> object
representing the desired mapping.  ShaderData objects provide a pointer to
the data as well as its size.
"""

import os
import re
import sys


def GetBasename(filename):
  """Returns the filename without the directory and without its extension.

  Args:
    filename: A filename from which a basename is to be extracted.

  Returns:
    A string derived from the filename except the directory and extension
    are removed.
  """
  return os.path.splitext(os.path.basename(filename))[0]


def GetHeaderDataDefinitionStringForFile(input_file):
  """Returns a string containing C++ that represents all file data in file."""
  with open(input_file, 'rb') as f:
    file_contents = f.read()
    def chunks(contents, chunk_size):
      """ Yield successive |chunk_size|-sized chunks from |contents|."""
      for i in xrange(0, len(contents), chunk_size):
        yield contents[i:i + chunk_size]

    # Break up the data into chunk sizes such that the produced output lines
    # representing the data in the .h file are less than 80 characters long.
    length_of_output_byte_string = 6
    max_characters_per_line = 80
    chunk_size = max_characters_per_line / length_of_output_byte_string

    # Convert each byte to ASCII hexadecimal form and output that to the C++
    # header file, line-by-line.
    data_definition_string = '{\n'
    for output_line_data in chunks(file_contents, chunk_size):
      data_definition_string += (
        ' '.join(['0x%02x,' % ord(y) for y in output_line_data]) + '\n')
    data_definition_string += '};\n\n'
    return data_definition_string


def GetHeaderDataDefinitionString(files):
  """Returns a string containing C++ that represents all file data in files."""

  data_definition_string = ''
  for path in files:
    input_file_variable_name = GetBasename(path)
    data_definition_string += (
        'const uint8_t %s[] =\n' % input_file_variable_name)
    data_definition_string += GetHeaderDataDefinitionStringForFile(path)

  return data_definition_string


def GetGenerateMapFunctionString(hash_to_shader_map):
  """Generate C++ code to populate a hash map from GLSL hash to shader data.

  Args:
    hash_to_shader_map:
      A dictionary where the keys are hashes and the values are
      platform-specific shader filenames.
  Returns:
    A string of C++ code that defines a function to setup a hash map where the
    keys are the GLSL hashes and the values are the platform-specific shader
    data.
  """

  generate_map_function_string = (
      'inline void GenerateGLSLShaderMap(GLSLShaderHashMap* out_map) {\n')

  for k, v in hash_to_shader_map.iteritems():
    input_file_variable_name = GetBasename(v)
    generate_map_function_string += (
        '  (*out_map)[%uU] = ShaderData(%s, sizeof(%s));\n' %
        (k, input_file_variable_name, input_file_variable_name))

  generate_map_function_string += '}'
  return generate_map_function_string



def GetShaderNameFunctionString(hash_to_shader_map):
  """Generate C++ code to retrieve the shader name from a GLSL hash.

  Args:
    hash_to_shader_map:
      A dictionary where the keys are hashes and the values are
      platform-specific shader filenames.
  Returns:
    A string of C++ code that defines a function that returns a string with the
    corresponding shader name for the GLSL hash value passed as a parameter.
  """

  get_shader_name_function_string = (
      'inline const char *GetShaderName(uint32_t hash_value) {\n'
      '  switch(hash_value) {\n')

  for k, v in hash_to_shader_map.iteritems():
    input_file_variable_name = GetBasename(v)
    get_shader_name_function_string += (
        '    case %uU: return \"%s\";\n' %
        (k, input_file_variable_name))

  get_shader_name_function_string += '  }\n  return NULL;\n}'
  return get_shader_name_function_string


HEADER_FILE_TEMPLATE = """
// Copyright 2016 Google Inc.  All Rights Reserved.
// This file is generated (by glimp/shaders/generate_glsl_shader_map.py).
// Do not edit!

#ifndef {include_guard}
#define {include_guard}

#include "glimp/shaders/glsl_shader_map_helpers.h"

namespace glimp {{
namespace shaders {{

{data_definitions}
{generate_map_function}

#if !defined(NDEBUG)
{shader_name_function}
#endif

}}  // namespace shaders
}}  // namespace glimp

#endif  // {include_guard}
"""

def GenerateHeaderFileOutput(output_file_name, hash_to_shader_map):
  """Generate the actual C++ header file code.

  Args:
    output_file_name: The filename for the output C++ header file.
    hash_to_shader_map:
      A Python dictionary whose keys are hashes and whose values are filenames
      for platform-specific shader files.
  """

  include_guard = 'GLIMP_SHADERS_GENERATED_GLSL_SHADER_MAP_H_'
  with open(output_file_name, 'w') as output_file:
    output_file.write(
        HEADER_FILE_TEMPLATE.format(
            include_guard = include_guard,
            data_definitions = GetHeaderDataDefinitionString(
                                   hash_to_shader_map.values()),
            generate_map_function = GetGenerateMapFunctionString(
                                        hash_to_shader_map),
            shader_name_function = GetShaderNameFunctionString(
                                       hash_to_shader_map)))


def AssociateGLSLFilesWithPlatformFiles(all_shaders):
  """Returns a dictionary mapping GLSL to platform shaders.

  all_shaders is an unorganized mixed list of both keys and values.
  This function reads it, identifies which items are GLSL files and which
  are platform shader files, and then associates GLSL files to platform shader
  files that share the same basename.

  Args:
    all_shaders: A list containing a mixture of GLSL and platform
                            shader filenames.
  Returns:
    A dictionary mapping GLSL shader filenames to platform-specific shader
    filenames.
  """
  def IsGLSL(filename):
    return os.path.splitext(filename)[1].upper() == '.GLSL'

  # First iterate through our list of files and determine which ones are keys.
  basename_to_glsl_file = {}
  for item in all_shaders:
    if IsGLSL(item):
      basename = GetBasename(item)
      if basename in basename_to_glsl_file:
        raise Exception('Multiple GLSL files match the same basename (' +
                        basename + ')')
      basename_to_glsl_file[basename] = item

  # Now iterate through again linking keys to values to create the map that we
  # will return.
  mapped_files = {}
  for item in all_shaders:
    if not IsGLSL(item):
      basename = GetBasename(item)
      if not basename in basename_to_glsl_file:
        raise Exception(
            'Platform-specific file ' + item + ' has no GLSL match.')
      glsl_file = basename_to_glsl_file[basename]
      if glsl_file in mapped_files:
        raise Exception('Multiple platform-specific files match the same GLSL '
                        + 'file with basename ' + basename)
      mapped_files[glsl_file] = item

  return mapped_files


def HashGLSLShaderFile(glsl_filename):
  """Opens the given GLSL file and returns a hash based on its contents.

  The contents of the specified file are read, and a hash is computed based on
  them.  This hash is computed in the same way that glimp's C++ code in
  hash_glsl_source.cc computes hashes so that hashes produced by this script
  can be later matched at runtime to a GLSL source.

  Note that all space, newline and tab characters are ignored when computing
  the hash.

  The hashing source is taken from the "one_at_a_time" hash function, by Bob
  Jenkins, in the public domain at http://burtleburtle.net/bob/hash/doobs.html.
  """
  with open(glsl_filename, 'r') as glsl_file:
    glsl_contents = glsl_file.read()

  def AddUint32(a, b):
    return (a + b) & 0xffffffff
  def XorUint32(a, b):
    return (a ^ b) & 0xffffffff

  hash_value = 0

  # Introduce a generator function for returning only the hashable characters.
  def GetHashableCharacters(hash_string):
    str_without_comments = re.sub(r'//.*\n', '', hash_string)
    str_without_whitespace = re.sub(r'[ \n\t]', '', str_without_comments);
    str_without_empty_braces = str_without_whitespace.replace('{}', '')

    for c in str_without_empty_braces:
      yield c

  for c in GetHashableCharacters(glsl_contents):
    hash_value = AddUint32(hash_value, ord(c))
    hash_value = AddUint32(hash_value, hash_value << 10)
    hash_value = XorUint32(hash_value, hash_value >> 6)
  hash_value = AddUint32(hash_value, hash_value << 3)
  hash_value = XorUint32(hash_value, hash_value >> 11)
  hash_value = AddUint32(hash_value, hash_value << 15)
  return hash_value


def CreateHashToShaderMap(glsl_to_shader_map):
  """Calculates hashes for each GLSL filename and replaces the keys with hashes.
  """
  hash_to_glsl_map = {}  # Used for reporting hash map collisions.
  hash_shader_map = {}
  for k, v in glsl_to_shader_map.iteritems():
    hashed_glsl = HashGLSLShaderFile(k)
    if hashed_glsl in hash_shader_map:
      raise Exception('Hash collision between GLSL files ' + k + ' and '
                      + hash_to_glsl_map[hashed_glsl] + '.')
    hash_to_glsl_map[hashed_glsl] = k
    hash_shader_map[hashed_glsl] = v
  return hash_shader_map


def GetAllShaderFiles(input_files_filename, input_files_dir):
  """Returns the list of all GLSL/platform shader files.

  Loads each line of |input_files_filename| into a list and then prepends entry
  with |input_files_dir|.
  """
  files = []
  with open(input_files_filename) as input_files_file:
    files = [x.strip() for x in input_files_file.readlines()]

  return [os.path.join(input_files_dir, x) for x in files]


def main(output_path, input_files_filename, input_files_dir):
  all_shaders = GetAllShaderFiles(input_files_filename, input_files_dir)

  glsl_to_shader_map = AssociateGLSLFilesWithPlatformFiles(
      all_shaders)

  hash_to_shader_map = CreateHashToShaderMap(glsl_to_shader_map)

  GenerateHeaderFileOutput(output_path, hash_to_shader_map)

if __name__ == '__main__':
  main(sys.argv[1], sys.argv[2], sys.argv[3])
