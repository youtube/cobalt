# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Generate a header for conversion of generated types.

Generate a header with forward declarations for conversion functions to and from
JavaScript values.
"""

from optparse import OptionParser
import os
import pickle

from utilities import ComponentInfoProviderCobalt
from utilities import write_file


def create_component_info_provider(interfaces_info_path, component_info_path):
  with open(os.path.join(interfaces_info_path)) as interface_info_file:
    interfaces_info = pickle.load(interface_info_file)
  with open(os.path.join(component_info_path)) as component_info_file:
    component_info = pickle.load(component_info_file)
  return ComponentInfoProviderCobalt(interfaces_info, component_info)


def parse_options():
  """Parse options needed for generating the header."""
  parser = OptionParser()
  parser.add_option(
      '--cache-directory', help='cache directory, defaults to output directory')
  parser.add_option('--component-info')
  parser.add_option('--interfaces-info')
  parser.add_option('--output-directory')
  options, args = parser.parse_args()

  assert not args
  return options


def generate_header(code_generator_class):
  """Generate a header with forward declarations for type conversions."""
  options = parse_options()

  # Create a CodeGeneratorCobalt, which will generate the file.
  cobalt_info_provider = create_component_info_provider(options.interfaces_info,
                                                        options.component_info)
  generator = code_generator_class(
      info_provider=cobalt_info_provider,
      cache_dir=options.cache_directory,
      output_dir=options.output_directory)

  # Generate the file and write it out to the specified path.
  header_path, header_text = generator.generate_conversion_code()
  write_file(header_text, header_path)
