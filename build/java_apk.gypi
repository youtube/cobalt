# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Android APKs in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_package_apk',
#   'type': 'none',
#   'variables': {
#     'package_name': 'my_package',
#     'apk_name': 'MyPackage',
#     'java_in_dir': 'path/to/package/root',
#     'resource_dir': 'res',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Note that this assumes that there's an ant buildfile <package_name>_apk.xml in
# java_in_dir. So, if you have package_name="content_shell" and
# java_in_dir="content/shell/android/java" you should have a directory structure
# like:
#
# content/shell/android/java/content_shell_apk.xml
# content/shell/android/java/src/chromium/base/Foo.java
# content/shell/android/java/src/chromium/base/Bar.java
#
# Required variables:
#  package_name - Used to name the intermediate output directory and in the
#    names of some output files.
#  apk_name - The final apk will be named <apk_name>-debug.apk (or -release)
#  java_in_dir - The top-level java directory. The src should be in
#    <java_in_dir>/src.
#  resource_dir - The directory for resources.
# Optional/automatic variables:
#  additional_input_paths - These paths will be included in the 'inputs' list to
#    ensure that this target is rebuilt when one of these paths changes.
#  additional_src_dirs - Additional directories with .java files to be compiled
#    and included in the output of this target.
#  asset_location - The directory where assets are located (default:
#    <PRODUCT_DIR>/<package_name>/assets).
#  generated_src_dirs - Same as additional_src_dirs except used for .java files
#    that are generated at build time. This should be set automatically by a
#    target's dependencies. The .java files in these directories are not
#    included in the 'inputs' list (unlike additional_src_dirs).
#  input_jars_paths - The path to jars to be included in the classpath. This
#    should be filled automatically by depending on the appropriate targets.
#  native_libs_paths - The path to any native library to be included in this
#    target.

{
  'variables': {
    'asset_location': '',
    'additional_input_paths': [],
    'input_jars_paths': [],
    'native_libs_paths': [],
    'additional_src_dirs': [],
    'generated_src_dirs': [],
  },
  'actions': [
    {
      'action_name': 'ant_<(package_name)_apk',
      'message': 'Building <(package_name) apk.',
      'inputs': [
        '<(java_in_dir)/AndroidManifest.xml',
        '<(DEPTH)/build/android/ant/chromium-apk.xml',
        '<(DEPTH)/build/android/ant/common.xml',
        '<(DEPTH)/build/android/ant/sdk-targets.xml',
        # If there is a separate find for additonal_src_dirs, it will find the
        # wrong .java files when additional_src_dirs is empty.
        '>!@(find >(java_in_dir) >(additional_src_dirs) -name "*.java")',
        '<!@(find <(java_in_dir)/<(resource_dir) -name "*")',
        '>@(input_jars_paths)',
        '>@(native_libs_paths)',
        '>@(additional_input_paths)',
      ],
      'outputs': [
        # TODO(cjhopman): Apks are built with a -debug suffix even when they are
        # built in release. This should be fixed.
        '<(PRODUCT_DIR)/apks/<(apk_name)-debug.apk',
      ],
      'action': [
        'ant',
        '-DAPP_ABI=<(android_app_abi)',
        '-DANDROID_GDBSERVER=<(android_gdbserver)',
        '-DANDROID_SDK=<(android_sdk)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DANDROID_TOOLCHAIN=<(android_toolchain)',
        '-DCHROMIUM_SRC=<(ant_build_out)/../..',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DPRODUCT_DIR=<(ant_build_out)',

        '-DAPK_NAME=<(apk_name)',
        '-DASSET_DIR=<(asset_location)',
        '-DADDITIONAL_SRC_DIRS=>(additional_src_dirs)',
        '-DGENERATED_SRC_DIRS=>(generated_src_dirs)',
        '-DINPUT_JARS_PATHS=>(input_jars_paths)',
        '-DPACKAGE_NAME=<(package_name)',
        '-DRESOURCE_DIR=<(resource_dir)',

        '-Dbasedir=<(java_in_dir)',
        '-buildfile',
        '<(DEPTH)/build/android/ant/chromium-apk.xml',

        # Specify CONFIGURATION_NAME as the target for ant to build. The
        # buildfile will then build the appropriate SDK tools target.
        '<(CONFIGURATION_NAME)',
      ]
    },
  ],
}
