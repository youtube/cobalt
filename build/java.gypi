# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Java in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_java',
#   'type': 'none',
#   'variables': {
#     'package_name': 'my-package',
#     'java_in_dir': 'path/to/package/root',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# The generated jar-file will be:
#   <(PRODUCT_DIR)/lib.java/chromium_<(package_name).jar
# Required variables:
#  package_name - Used to name the intermediate output directory and in the
#    names of some output files.
#  java_in_dir - The top-level java directory. The src should be in
#    <java_in_dir>/src.
# Optional/automatic variables:
#  additional_input_paths - These paths will be included in the 'inputs' list to
#    ensure that this target is rebuilt when one of these paths changes.
#  additional_src_dirs - Additional directories with .java files to be compiled
#    and included in the output of this target.
#  generated_src_dirs - Same as additional_src_dirs except used for .java files
#    that are generated at build time. This should be set automatically by a
#    target's dependencies. The .java files in these directories are not
#    included in the 'inputs' list (unlike additional_src_dirs).
#  input_jars_paths - The path to jars to be included in the classpath. This
#    should be filled automatically by depending on the appropriate targets.
#  javac_includes - A list of specific files to include. This is by default
#    empty, which leads to inclusion of all files specified. May include
#    wildcard, and supports '**/' for recursive path wildcards, ie.:
#    '**/MyFileRegardlessOfDirectory.java', '**/IncludedPrefix*.java'.
#  has_java_resources - Set to 1 if the java target contains an
#    Android-compatible resources folder named res.  If 1, R_package and
#    R_package_relpath must also be set.
#  R_package - The java package in which the R class (which maps resources to
#    integer IDs) should be generated, e.g. org.chromium.content.
#  R_package_relpath - Same as R_package, but replace each '.' with '/'.

{
  'dependencies': [
    '<(DEPTH)/build/build_output_dirs_android.gyp:build_output_dirs'
  ],
  # This all_dependent_settings is used for java targets only. This will add the
  # chromium_<(package_name) jar to the classpath of dependent java targets.
  'all_dependent_settings': {
    'variables': {
      'input_jars_paths': ['<(PRODUCT_DIR)/lib.java/chromium_<(package_name).jar'],
    },
  },
  'variables': {
    'input_jars_paths': [],
    'additional_src_dirs': [],
    'javac_includes': [],
    'additional_input_paths': ['>@(additional_R_files)'],
    'generated_src_dirs': ['>@(generated_R_dirs)'],
    'generated_R_dirs': [],
    'additional_R_files': [],
    'has_java_resources%': 0,
  },
  'conditions': [
    ['has_java_resources == 1', {
      'variables': {
        'res_dir': '<(java_in_dir)/res',
        'crunched_res_dir': '<(SHARED_INTERMEDIATE_DIR)/<(package_name)/res',
        'R_dir': '<(SHARED_INTERMEDIATE_DIR)/<(package_name)/java_R',
        'R_file': '<(R_dir)/<(R_package_relpath)/R.java',
        'generated_src_dirs': ['<(R_dir)'],
        'additional_input_paths': ['<(R_file)'],
      },
      'all_dependent_settings': {
        'variables': {
          # Dependent jars include this target's R.java file via
          # generated_R_dirs and additional_R_files.
          'generated_R_dirs': ['<(R_dir)'],
          'additional_R_files': ['<(R_file)'],

          # Dependent APKs include this target's resources via
          # additional_res_dirs and additional_res_packages.
          'additional_res_dirs': ['<(crunched_res_dir)', '<(res_dir)'],
          'additional_res_packages': ['<(R_package)'],
        },
      },
      'actions': [
        # Generate R.java and crunch image resources.
        {
          'action_name': 'process_resources',
          'message': 'processing resources for <(package_name)',
          'inputs': [
            '<(DEPTH)/build/android/process_resources.py',
            '<!@(find <(res_dir) -type f)',
          ],
          'outputs': [
            '<(R_file)',
          ],
          'action': [
            '<(DEPTH)/build/android/process_resources.py',
            '--android-sdk', '<(android_sdk)',
            '--android-sdk-tools', '<(android_sdk_tools)',
            '--R-package', '<(R_package)',
            '--R-dir', '<(R_dir)',
            '--res-dir', '<(res_dir)',
            '--crunched-res-dir', '<(crunched_res_dir)',
          ],
        },
      ],
    }],
  ],
  'actions': [
    {
      'action_name': 'ant_<(package_name)',
      'message': 'Building <(package_name) java sources.',
      'inputs': [
        'android/ant/common.xml',
        'android/ant/chromium-jars.xml',
        '>!@(find >(java_in_dir) >(additional_src_dirs) -name "*.java")',
        '>@(input_jars_paths)',
        '>@(additional_input_paths)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/lib.java/chromium_<(package_name).jar',
      ],
      'action': [
        'ant',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DANDROID_SDK=<(android_sdk)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DANDROID_GDBSERVER=<(android_gdbserver)',
        '-DPRODUCT_DIR=<(ant_build_out)',

        '-DADDITIONAL_SRC_DIRS=>(additional_src_dirs)',
        '-DGENERATED_SRC_DIRS=>(generated_src_dirs)',
        '-DINPUT_JARS_PATHS=>(input_jars_paths)',
        '-DPACKAGE_NAME=<(package_name)',
        '-DJAVAC_INCLUDES=>(javac_includes)',

        '-Dbasedir=<(java_in_dir)',
        '-buildfile',
        '<(DEPTH)/build/android/ant/chromium-jars.xml'
      ]
    },
  ],
}
