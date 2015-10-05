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
    'cobalt_code': 1,
    'static_contents_dir': '<(DEPTH)/lbshell/content',
  },
  'targets': [
    {
      'target_name': 'network',
      'type': 'static_library',
      'sources': [
        '<(actual_target_arch)/network_system.cc',
        'network_delegate.cc',
        'network_delegate.h',
        'network_module.cc',
        'network_module.h',
        'network_system.h',
        'persistent_cookie_store.cc',
        'persistent_cookie_store.h',
        'url_request_context.cc',
        'url_request_context.h',
        'url_request_context_getter.cc',
        'url_request_context_getter.h',
        'user_agent.cc',
        'user_agent.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/storage/storage.gyp:storage',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/net/net.gyp:net',
      ],
      'copies': [
      {
        'destination': '<(PRODUCT_DIR)/content/data/ssl',
        'files': ['<(static_contents_dir)/ssl/certs/'],
      }],
    },
    {
      'target_name': 'network_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'persistent_cookie_store_test.cc',
        'user_agent_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/storage/storage.gyp:storage_test_utils',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'network',
      ],
    },
    {
      'target_name': 'network_test_deploy',
      'type': 'none',
      'dependencies': [
        'network_test',
      ],
      'variables': {
        'executable_name': 'network_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },
  ],
}
