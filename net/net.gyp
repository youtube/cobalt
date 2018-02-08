# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'net',
      'type': '<(component)',
      'defines': [
        'NET_IMPLEMENTATION',
        'DISABLE_FTP_SUPPORT',
        'ENABLE_BUILT_IN_DNS',
      ],
      'direct_dependent_settings': {
        'defines': [
          'DISABLE_FTP_SUPPORT',
          'ENABLE_BUILT_IN_DNS',
        ],
      },
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/url/url.gyp:url',
        '<(DEPTH)/nb/nb.gyp:nb',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/openssl/openssl.gyp:openssl',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
      ],
    },
    {
      'target_name': 'net_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/url/url.gyp:url',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/third_party/openssl/openssl.gyp:openssl',
        'http_server',  # This is needed by dial_http_server in net
        'net',
      ],
      'sources': [
      ],
      'actions': [
        {
          'action_name': 'copy_test_data',
          'variables': {
            'input_files': [
              'test/data',
            ],
            'output_dir': 'net/test',
          },
          'includes': [ '../cobalt/build/copy_test_data.gypi' ],
        },
      ],
    },
    {
      'target_name': 'http_server',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'net',
      ],
      'sources': [
      ],
    },
    {
      'target_name': 'net_unittests_deploy',
      'type': 'none',
      'dependencies': [
        'net_unittests',
      ],
      'variables': {
        'executable_name': 'net_unittests',
      },
      'includes': [ '../starboard/build/deploy.gypi' ],
    },
  ],
}
