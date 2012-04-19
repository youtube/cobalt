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
              'action_name': 'native_test_apk',
              'inputs': [
                '<(DEPTH)/testing/android/native_test_apk.xml',
                '<!@(find <(DEPTH)/testing/android -name "*.java")',
                'native_test_launcher.cc'
              ],
              'outputs': [
                # Awkwardly, we build a Debug APK even when gyp is in
                # Release mode.  I don't think it matters (e.g. we're
                # probably happy to not codesign) but naming should be
                # fixed.  The -debug name is an aspect of the android
                # SDK antfiles (e.g. ${sdk.dir}/tools/ant/build.xml)
                '<(PRODUCT_DIR)/ChromeNativeTests-debug.apk',
              ],
              'action': [
                'ant',
                '-DPRODUCT_DIR=<(PRODUCT_DIR)',
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
            'jni_headers',
            '../../base/base.gyp:base',
            '../../base/base.gyp:test_support_base',
            '../gtest.gyp:gtest',
          ],
        },
        {
          'target_name': 'jni_headers',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate_jni_headers',
              'inputs': [
                '../../base/android/jni_generator/jni_generator.py',
                'java/src/org/chromium/native_test/ChromeNativeTestActivity.java'
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/testing/android/jni/'
                'chrome_native_test_activity_jni.h',
              ],
              'action': [
                'python',
                '../../base/android/jni_generator/jni_generator.py',
                '-o',
                '<@(_inputs)',
                '<@(_outputs)',
              ],
            }
          ],
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
