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
# Note that this assumes that there's ant buildfile with package_name in
# java_in_dir. So if if you have package_name="base" and
# java_in_dir="base/android/java" you should have a directory structure like:
#
# base/android/java/base.xml
# base/android/java/org/chromium/base/Foo.java
# base/android/java/org/chromium/base/Bar.java
#
# Finally, the generated jar-file will be:
#   <(PRODUCT_DIR)/lib.java/chromium_base.jar
#
# TODO(yfriedman): The "finally" statement isn't entirely true yet, as we don't
# auto-generate the ant file yet.

{
  'actions': [
    {
      'action_name': 'ant_<(package_name)',
      'message': 'Building <(package_name) java sources.',
      'inputs': [
         '<(java_in_dir)/<(package_name).xml',
         '<!@(find <(java_in_dir) -name "*.java")'
      ],
      'outputs': [
        '<(PRODUCT_DIR)/lib.java/chromium_<(package_name).jar',
      ],
      'action': [
        'ant',
        '-DPRODUCT_DIR=<(PRODUCT_DIR)',
        '-DPACKAGE_NAME=<(package_name)',
        '-buildfile',
        '<(java_in_dir)/<(package_name).xml',
      ]
    },
  ],
}
