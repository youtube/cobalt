# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'native_test_apk',
          'message': 'building native test apk',
          'type': 'none',
          'dependencies': [
            'native_test_native_code',
          ],
          'actions': [
            {
              'action_name': 'copy_base_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_base.jar'],
              'outputs': ['<(PRODUCT_DIR)/replaceme_apk/java/libs/chromium_base.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'native_test_apk',
              'inputs': [
                '<(DEPTH)/testing/android/native_test_apk.xml',
                '<!@(find <(DEPTH)/testing/android -name "*.java")',
                '<(PRODUCT_DIR)/replaceme_apk/java/libs/chromium_base.jar',
                'native_test_launcher.cc'
              ],
              'outputs': [
                # Awkwardly, we build a Debug APK even when gyp is in
                # Release mode.  I don't think it matters (e.g. we're
                # probably happy to not codesign) but naming should be
                # fixed.  The -debug name is an aspect of the android
                # SDK antfiles (e.g. ${sdk.dir}/tools/ant/build.xml)
                '<(PRODUCT_DIR)/replaceme_apk/replaceme-debug.apk',
              ],
              'action': [
                'ant',
                # TODO: All of these paths are absolute paths right now, while
                # we really should be using relative paths for anything that is
                # checked in to the Chromium tree (among which the SDK).
                '-DPRODUCT_DIR=<(ant_build_out)',
                '-DANDROID_SDK=<(android_sdk)',
                '-DANDROID_SDK_ROOT=<(android_sdk_root)',
                '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
                '-DANDROID_SDK_VERSION=<(android_sdk_version)',
                '-DANDROID_TOOLCHAIN=<(android_toolchain)',
                '-DCHROMIUM_SRC=<(ant_build_out)/../..',
                '-buildfile',
                '<(DEPTH)/testing/android/native_test_apk.xml',
              ]
            }
          ],
        },
        {
          'target_name': 'native_test_native_code',
          'message': 'building native pieces of native test package',
          'type': 'static_library',
          'sources': [
            'native_test_launcher.cc',
          ],
          'direct_dependent_settings': {
            'ldflags!': [
              # JNI_OnLoad is implemented in a .a and we need to
              # re-export in the .so.
              '-Wl,--exclude-libs=ALL',
            ],
          },
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/base.gyp:test_support_base',
            '../gtest.gyp:gtest',
            'native_test_jni_headers',
          ],
        },
        {
          'target_name': 'native_test_jni_headers',
          'type': 'none',
          'sources': [
            'java/src/org/chromium/native_test/ChromeNativeTestActivity.java'
          ],
          'variables': {
            'jni_gen_dir': 'testing',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
          # So generated jni headers can be found by targets that
          # depend on this.
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
        },
      ],
    }]
  ],
}
