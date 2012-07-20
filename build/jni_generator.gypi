# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to generate jni bindings for Java-files in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'base_jni_headers',
#   'type': 'none',
#   'variables': {
#       'java_sources': [
#         'android/java/org/chromium/base/BuildInfo.java',
#         'android/java/org/chromium/base/PathUtils.java',
#         'android/java/org/chromium/base/SystemMessageHandler.java',
#       ],
#       'jni_headers': [
#         '<(SHARED_INTERMEDIATE_DIR)/base/jni/build_info_jni.h',
#         '<(SHARED_INTERMEDIATE_DIR)/base/jni/path_utils_jni.h',
#         '<(SHARED_INTERMEDIATE_DIR)/base/jni/system_message_handler_jni.h',
#       ],
#   },
#   'includes': [ '../build/jni_generator.gypi' ],
# }
#
# The ordering of the java_sources must match the ordering of jni_headers. The
# result is that for each Java file listed in java_sources, the corresponding
# entry in jni_headers contains the JNI bindings produced from running the
# jni_generator on the input file.
#
# See base/android/jni_generator/jni_generator.py for more info about the
# format of generating JNI bindings.

{
  'actions': [
      {
        'action_name': 'generate_jni_headers',
        'type': 'none',
        'inputs': [
          '<(DEPTH)/base/android/jni_generator/jni_generator.py',
          '<@(java_sources)',
        ],
        'outputs': [
          '<@(jni_headers)',
        ],
        'action': [
          'python',
          '<(DEPTH)/base/android/jni_generator/jni_generator.py',
          '-o',
          '<@(_inputs)',
          '<@(_outputs)',
        ],
      },
  ],
}
