# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is meant to be included into a target to provide an action
# to compute information about individual interfaces defined in a component.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'interfaces_info_individual_component',
#   'dependencies': [
#     'generated_idls_target',
#   ],
#   'variables': {
#     'static_idl_files': '<(component_static_idl_files)',
#     'generated_idl_files': '<(component_generated_idl_files)',
#     'output_file':
#       '<(bindings_core_output_dir)/InterfacesInfoComponentIndividual.pickle',
#   },
#   'includes': ['path/to/this/gypi/file'],
# },
#
# Required variables:
#  static_idl_files - All static .idl files for the component, including
#    dependencies and testing.
#  generated_idl_files - All generated .idl files for the component.
#    (Must be separate from static because build dir not know at gyp time.)
#  interfaces_info_file - Output pickle file containing interfaces info.
#  component_info_file - Output pickle file containing component-wide info.
#
# Design document: http://www.chromium.org/developers/design-documents/idl-build

{
  'type': 'none',
  'actions': [{
    'action_name': 'compute_<(_target_name)',
    'message': 'Computing global information about individual IDL files for <(_target_name)',
    'variables': {
      'static_idl_files_list':
        '<|(<(_target_name)_static_idl_files_list.tmp <@(static_idl_files))',
    },
    'inputs': [
      '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
      '<(bindings_scripts_dir)/utilities.py',
      '<(extended_attributes_file)',
      '<(static_idl_files_list)',
      '<@(static_idl_files)',
      '<@(generated_idl_files)',
      '<@(dependency_idl_files)',
    ],
    'outputs': [
      '<(interfaces_info_file)',
      '<(component_info_file)',
    ],

    'action': [
      'python2',
      '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
      '--cache-directory',
      '<(cache_directory)',
      '--root-directory',
      '<(root_directory)',
      '--idl-files-list',
      '<(static_idl_files_list)',
      '--dependency-idl-files',
      '<(dependency_idl_files)',
      '--interfaces-info-file',
      '<(interfaces_info_file)',
      '--component-info-file',
      '<(component_info_file)',
      '--extended-attributes',
      '<(extended_attributes_file)',
      '--',
      # Generated files must be passed at command line
      '<@(generated_idl_files)',
    ],
  }],
}
