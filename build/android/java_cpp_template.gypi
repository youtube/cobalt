# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to generate Java source files from templates that are processed
# through the host C pre-processor.
#
# This assumes a GNU-compatible pre-processor installed as 'cpp'.
# Only tested on Linux.
#
# To use this, create a gyp target with the following form:
#  {
#    'target_name': 'android_net_java_constants',
#    'type': 'none',
#    'sources': [
#      'net/android/NetError.template',
#    ],
#    'variables': {
#      'package_name': 'org.chromium.net',
#      'template_deps': ['net/base/certificate_mime_type_list.h'],
#    },
#    'includes': [ '../build/android/java_constants.gypi' ],
#  },
#
# The 'sources' entry should only list template file. The template file
# itself should use the 'ClassName.template' format, and will generate
# 'gen/templates/<package-name>/ClassName.java. The files which template
# dependents on and typically included by the template should be listed
# in template_deps variables. Any change to them will force a rebuild of
# the template, and hence of any source that depends on it.
#

{
  # Location where all generated Java sources will be placed.
  'variables': {
    'output_dir': '<(SHARED_INTERMEDIATE_DIR)/templates/<(package_name)'
  },
  # Ensure that the output directory is used in the class path
  # when building targets that depend on this one.
  'direct_dependent_settings': {
    'variables': {
      'generated_src_dirs': [
        '<(output_dir)/',
      ],
    },
  },
  # Define a single rule that will be apply to each .template file
  # listed in 'sources'.
  'rules': [
    {
      'rule_name': 'generate_java_constants',
      'extension': 'template',
      # Set template_deps as additional dependencies.
      'inputs': ['<@(template_deps)'],
      'outputs': [
        '<(output_dir)/<(RULE_INPUT_ROOT).java'
      ],
      'action': [
        'cpp',                 # invoke host pre-processor.
        '-x', 'c-header',      # treat sources as C header files
        '-P',                  # disable line markers, i.e. '#line 309'
        '-I', '<(DEPTH)',      # Add project top-level to include path
        '-o', '<@(_outputs)',  # Specify output file
        '<(RULE_INPUT_PATH)',  # Specify input file
      ],
      'message': 'Generating Java from cpp template <(RULE_INPUT_PATH)',
    }
  ],
}
