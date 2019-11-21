# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'crypto_impl',
      'type': 'static_library',
      'sources': [
        'crypto_impl.cc',
        'crypto_impl.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/boringssl/boringssl.gyp:crypto',
      ]
    },
    {
      'target_name': 'crypto_impl_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'crypto_impl_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'crypto_impl',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },
    {
      'target_name': 'subtlecrypto',
      'type': 'static_library',
      'sources': [
        'subtle_crypto.cc',
        'subtle_crypto.h',
        'crypto_key.cc',
        'crypto_key.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        'crypto_impl',
      ],
    },
    {
      'target_name': 'crypto_impl_test_deploy',
      'type': 'none',
      'dependencies': [
        'crypto_impl_test',
      ],
      'variables': {
        'executable_name': 'crypto_impl_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ]
}

