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
"""Compile an .idl file to Cobalt bindings (.h and .cpp files).

Based on and uses blink's IDL compiler found in the blink repository at
third_party/blink/Source/bindings/scripts/idl_compiler.py.
"""

from optparse import OptionParser
import os
import pickle

import _env  # pylint: disable=unused-import
from idl_compiler import IdlCompiler
from utilities import ComponentInfoProviderCobalt
from utilities import idl_filename_to_interface_name
from utilities import write_file


def create_component_info_provider(interfaces_info_path, component_info_path):
  with open(os.path.join(interfaces_info_path)) as interface_info_file:
    interfaces_info = pickle.load(interface_info_file)
  with open(os.path.join(component_info_path)) as component_info_file:
    component_info = pickle.load(component_info_file)
  return ComponentInfoProviderCobalt(interfaces_info, component_info)


def parse_options():
  """Parse command line options. Based on parse_options in idl_compiler.py."""
  parser = OptionParser()
  parser.add_option(
      '--cache-directory', help='cache directory, defaults to output directory')
  parser.add_option('--output-directory')
  parser.add_option('--interfaces-info')
  parser.add_option('--component-info')
  parser.add_option(
      '--extended-attributes',
      help='file containing whitelist of supported extended attributes')

  options, args = parser.parse_args()
  if options.output_directory is None:
    parser.error('Must specify output directory using --output-directory.')
  if len(args) != 1:
    parser.error('Must specify exactly 1 input file as argument, but %d given.'
                 % len(args))
  idl_filename = os.path.realpath(args[0])
  return options, idl_filename


class IdlCompilerCobalt(IdlCompiler):

  def __init__(self, *args, **kwargs):
    IdlCompiler.__init__(self, *args, **kwargs)

  def compile_and_write(self, idl_filename):
    interface_name = idl_filename_to_interface_name(idl_filename)
    definitions = self.reader.read_idl_definitions(idl_filename)
    # Merge all component definitions into a single list, since we don't really
    # care about components in Cobalt.
    assert len(definitions) == 1
    output_code_list = self.code_generator.generate_code(
        definitions.values()[0], interface_name)

    # Generator may choose to omit the file.
    if output_code_list is None:
      return

    for output_path, output_code in output_code_list:
      write_file(output_code, output_path)


def generate_bindings(code_generator_class):
  """This will be called from the engine-specific idl_compiler.py script."""
  options, input_filename = parse_options()
  # |input_filename| should be a path of an IDL file.
  cobalt_info_provider = create_component_info_provider(options.interfaces_info,
                                                        options.component_info)
  idl_compiler = IdlCompilerCobalt(
      options.output_directory,
      cache_directory=options.cache_directory,
      code_generator_class=code_generator_class,
      info_provider=cobalt_info_provider,
      extended_attributes_filepath=options.extended_attributes)
  idl_compiler.compile_file(input_filename)
