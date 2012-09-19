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

{
  'variables': {
    'input_jars_paths': [],
    'native_libs_paths': [],
    'additional_src_dirs': [],
    'additional_input_paths': [],
  },
  'actions': [
    {
      'action_name': 'ant_<(package_name)_apk',
      'message': 'Building <(package_name) apk.',
      'inputs': [
        '<(java_in_dir)/<(package_name)_apk.xml',
        '<(java_in_dir)/AndroidManifest.xml',
        '<(DEPTH)/build/android/ant/common.xml',
        '<(DEPTH)/build/android/ant/sdk-targets.xml',
        '<!@(find <(java_in_dir) -name "*.java")',
        '<!@(find <(java_in_dir)/<(resource_dir) -name "*")',
        '>@(input_jars_paths)',
        '>@(native_libs_paths)',
        '>@(additional_input_paths)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/apks/<(apk_name)-debug.apk',
      ],
      'action': [
        'ant',
        '-DPRODUCT_DIR=<(ant_build_out)',
        '-DAPP_ABI=<(android_app_abi)',
        '-DANDROID_GDBSERVER=<(android_gdbserver)',
        '-DANDROID_SDK=<(android_sdk)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DANDROID_TOOLCHAIN=<(android_toolchain)',
        '-DPACKAGE_NAME=<(package_name)',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DINPUT_JARS_PATHS=>(input_jars_paths)',
        '-DADDITIONAL_SRC_DIRS=>(additional_src_dirs)',
        '-DCHROMIUM_SRC=<(ant_build_out)/../..',
        '-DRESOURCE_DIR=<(resource_dir)',
        '-buildfile',
        '<(java_in_dir)/<(package_name)_apk.xml'
      ]
    },
  ],
}
