# Copyright 2014 Google Inc. All Rights Reserved.
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

"""Compile an .idl file to Cobalt JSC bindings (.h and .cpp files).

Based on and uses blink's IDL compiler found in the blink repository at
third_party/blink/Source/bindings/scripts/idl_compiler.py
"""

import os
import sys

module_path, module_filename = os.path.split(os.path.realpath(__file__))
# Adding this to the path allows us to import from blink's bindings.scripts
blink_source_dir = os.path.normpath(os.path.join(module_path,
                                                 os.pardir,
                                                 os.pardir,
                                                 'third_party',
                                                 'blink',
                                                 'Source'))
sys.path.append(blink_source_dir)

from bindings.scripts.idl_compiler import IdlCompiler  # pylint: disable=g-import-not-at-top
from bindings.scripts.idl_compiler import parse_options
from code_generator_cobalt import CodeGeneratorCobalt


class IdlCompilerCobalt(IdlCompiler):

  def __init__(self, *args, **kwargs):
    IdlCompiler.__init__(self, *args, **kwargs)
    self.code_generator = CodeGeneratorCobalt(self.interfaces_info,
                                              self.cache_directory,
                                              self.output_directory)

  def compile_file(self, idl_filename):
    self.compile_and_write(idl_filename)


def generate_bindings(options, input_filename):
  idl_compiler = IdlCompilerCobalt(
      options.output_directory,
      cache_directory=options.cache_directory,
      interfaces_info_filename=options.interfaces_info_file,
      only_if_changed=options.write_file_only_if_changed,
      target_component=options.target_component,
      extended_attributes_filepath=options.extended_attributes)
  idl_compiler.compile_file(input_filename)


def main():
  options, input_filename = parse_options()
  if options.generate_impl:
    # Currently not used in Cobalt
    raise NotImplementedError
  else:
    # |input_filename| should be a path of an IDL file.
    generate_bindings(options, input_filename)


if __name__ == '__main__':
  sys.exit(main())
