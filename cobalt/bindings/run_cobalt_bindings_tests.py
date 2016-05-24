#!/usr/bin/python
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
"""Run bindings pipeline on test files and compare results with reference files.

Please execute the script whenever changes are made to the compiler
(this is automatically done as a presubmit script),
and submit changes to the test results in the same patch.
This makes it easier to track and review changes in generated code.
Options:
   --reset-results: Overwrites reference files with the generated results.
   --verbose: Show output on success and logging messages (not just failure)
"""
import argparse
import os
import sys
module_path, module_filename = os.path.split(os.path.realpath(__file__))
cobalt_src_root = os.path.normpath(os.path.join(module_path, os.pardir,
                                                os.pardir))
# Add blink's python tools to the path.
blink_tools_scripts_dir = os.path.normpath(os.path.join(
    cobalt_src_root, 'third_party', 'blink', 'Tools', 'Scripts'))
sys.path.append(blink_tools_scripts_dir)
cobalt_bindings_dir = os.path.normpath(os.path.join(cobalt_src_root, 'cobalt',
                                                    'bindings'))
# Add cobalt's bindings generation scripts to the path.
sys.path.append(cobalt_bindings_dir)
from idl_compiler_cobalt import IdlCompilerCobalt  # pylint: disable=g-import-not-at-top
from javascriptcore.code_generator import CodeGeneratorJsc
from mozjs.code_generator import CodeGeneratorMozjs
from webkitpy.bindings.main import run_bindings_tests
extended_attributes_path = os.path.join(cobalt_src_root, 'cobalt', 'bindings',
                                        'IDLExtendedAttributes.txt')


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('-r', '--reset-results', action='store_true')
  parser.add_argument('-v', '--verbose', action='store_true')
  parser.add_argument('engine')
  args = parser.parse_args(argv[1:])

  if args.engine.lower() == 'jsc':
    generator = CodeGeneratorJsc
  elif args.engine.lower() == 'mozjs':
    generator = CodeGeneratorMozjs
  else:
    raise RuntimeError('Unsupported JavaScript engine: ' + args.engine)

  test_input_directory = os.path.join(cobalt_bindings_dir)
  reference_directory = os.path.join(cobalt_bindings_dir, 'generated',
                                     args.engine)
  test_args = {
      'idl_compiler_constructor': IdlCompilerCobalt,
      'code_generator_constructor': generator,

      # Directory that contains IDL files for the bindings tests
      'test_input_directory': test_input_directory,
      # Directory that contains reference generated files
      'reference_directory': reference_directory,
      # Top-level directories containing IDL files. In blink, there are 'core'
      # and 'modules'. These correspond to the subdirectories under Source/core
      # and Source/modules in the blink repository. These are separated to
      # enforce dependency restrictions. For example, for interface X and
      # partial interface Y that extends X, the following are valid:
      #   X and Y are both in core
      #   X and Y are both in modules
      #   X is in core and Y is in modules
      # The following is invalid
      #   X is in modules and Y is in core
      # TODO(***REMOVED***): Evaluate the usefulness of this distinction in Cobalt.
      # This promotes modularity, but necessarily introduces complexity.
      'component_directories': frozenset(['testing']),
      # These are IDL files that aren't built directly, but are used by their
      # dependent IDLs. In blink this includes:
      #   interfaces with the NoInterfaceObject extended attributes
      #   partial interfaces
      # This should be kept in sync with the list of dependency IDLS in
      # testing.gyp.
      'dependency_idl_files': frozenset([
          'ImplementedInterface.idl',
          'PartialInterface.idl',
          'InterfaceWithUnsupportedProperties_partial.idl',
          # UnsupportedInterface is not actually a dependency IDL, but this will
          # prevent code from being generated for it.
          'UnsupportedInterface.idl',
      ]),
      # .idl files that are not valid blink IDLs
      'ignore_idl_files': frozenset([]),
      # Use as the base for constructing relative paths for includes, etc.
      'root_directory': cobalt_src_root,
      # Text file containing whitelisted extended attributes.
      'extended_attributes_path': extended_attributes_path,
      # We implement union types differently than blink, so don't generate
      # Blink union classes.
      'generate_union_containers': False,
  }
  return run_bindings_tests(args.reset_results, args.verbose, test_args)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
