# Copyright 2015 Google Inc. All Rights Reserved.
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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'csp',
      'type': 'static_library',
      'sources': [
        'content_security_policy.cc',
        'content_security_policy.h',
        'crypto.cc',
        'crypto.h',
        'directive.cc',
        'directive.h',
        'directive_list.cc',
        'directive_list.h',
        'media_list_directive.cc',
        'media_list_directive.h',
        'parsers.h',
        'source.cc',
        'source.h',
        'source_list.cc',
        'source_list.h',
        'source_list_directive.cc',
        'source_list_directive.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'csp_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'content_security_policy_test.cc',
        'source_list_test.cc',
        'source_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'csp',
        'csp_copy_test_data',
      ],
    },

    {
      'target_name': 'csp_test_deploy',
      'type': 'none',
      'dependencies': [
        'csp_test',
      ],
      'variables': {
        'executable_name': 'csp_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'csp_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/cobalt/csp/testdata/',
        ],
        'content_test_output_subdir': 'cobalt/csp/testdata/',
      },
      'includes': [ '<(DEPTH)/starboard/build/copy_test_data.gypi' ],
    },
  ],
}
