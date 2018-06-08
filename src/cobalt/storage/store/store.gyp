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
  'targets': [
    {
      'target_name': 'memory_store',
      'type': 'static_library',
      'sources': [
        'memory_store.h',
        'memory_store.cc',
        'storage.pb.cc',
        'storage.pb.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/storage/storage_constants.gyp:storage_constants',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'include_dirs': [
        # Get protobuf headers from the chromium tree.
        '<(DEPTH)/third_party/protobuf/src',
      ],
      'defines': [
        # The generated code needs to be compiled with the same flags as the protobuf library.
        # Otherwise we get static initializers which are not thread safe.
        # This macro must be defined to suppress the use of dynamic_cast<>,
        # which requires RTTI.
        'GOOGLE_PROTOBUF_NO_RTTI',
        'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
        'HAVE_PTHREAD',
      ],
    },
    {
      'target_name': 'memory_store_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'memory_store_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'memory_store',
      ],
      'include_dirs': [
        # Get protobuf headers from the chromium tree.
        '<(DEPTH)/third_party/protobuf/src',
      ],
      'defines': [
        # The generated code needs to be compiled with the same flags as the protobuf library.
        # Otherwise we get static initializers which are not thread safe.
        # This macro must be defined to suppress the use of dynamic_cast<>,
        # which requires RTTI.
        'GOOGLE_PROTOBUF_NO_RTTI',
        'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
        'HAVE_PTHREAD',
      ],
    },
    {
      'target_name': 'memory_store_test_deploy',
      'type': 'none',
      'dependencies': [
        'memory_store_test',
      ],
      'variables': {
        'executable_name': 'memory_store_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
