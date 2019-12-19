# Copyright 2018 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'url',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/icu/icu.gyp:icudata',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        'gurl.cc',
        'gurl.h',
        'origin.cc',
        'origin.h',
        'scheme_host_port.cc',
        'scheme_host_port.h',
        'third_party/mozilla/url_parse.cc',
        'third_party/mozilla/url_parse.h',
        'url_canon.cc',
        'url_canon.h',
        'url_canon_etc.cc',
        'url_canon_filesystemurl.cc',
        'url_canon_fileurl.cc',
        'url_canon_host.cc',
        'url_canon_icu.cc',
        'url_canon_icu.h',
        'url_canon_internal.cc',
        'url_canon_internal.h',
        'url_canon_internal_file.h',
        'url_canon_ip.cc',
        'url_canon_ip.h',
        'url_canon_mailtourl.cc',
        'url_canon_path.cc',
        'url_canon_pathurl.cc',
        'url_canon_query.cc',
        'url_canon_relative.cc',
        'url_canon_stdstring.cc',
        'url_canon_stdstring.h',
        'url_canon_stdurl.cc',
        'url_constants.cc',
        'url_constants.h',
        'url_export.h',
        'url_file.h',
        'url_idna_icu.cc',
        'url_parse_file.cc',
        'url_parse_internal.h',
        'url_util.cc',
        'url_util.h',
        'url_util_internal.h',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'defines': [
            'GURL_DLL',
            'GURL_IMPLEMENTATION=1',
          ],
          'direct_dependent_settings': {
            'defines': [
              'GURL_DLL',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'url_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        'url',
      ],
      'sources': [
        'gurl_unittest.cc',
        'origin_unittest.cc',
        'run_all_unittests.cc',
        'scheme_host_port_unittest.cc',
        'url_canon_icu_unittest.cc',
        'url_canon_unittest.cc',
        'url_parse_unittest.cc',
        'url_test_utils.h',
        'url_util_unittest.cc',
      ],
    },
  ],
}
