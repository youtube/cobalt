# Copyright 2019 Google Inc. All Rights Reserved.
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
      'target_name': 'nb_test',
      'type': '<(gtest_target_type)',
      'conditions': [
        ['OS=="starboard"', {
          'sources': [
            'analytics/memory_tracker_helpers_test.cc',
            'analytics/memory_tracker_impl_test.cc',
            'analytics/memory_tracker_test.cc',
            'bidirectional_fit_reuse_allocator_test.cc',
            'concurrent_map_test.cc',
            'concurrent_ptr_test.cc',
            'first_fit_reuse_allocator_test.cc',
            'fixed_no_free_allocator_test.cc',
            'memory_scope_test.cc',
            'multipart_allocator_test.cc',
            'rewindable_vector_test.cc',
            'run_all_unittests.cc',
            'simple_profiler_test.cc',
            'std_allocator_test.cc',
            'string_interner_test.cc',
            'test_thread.h',
            'thread_local_object_test.cc',
          ],
          'dependencies': [
            '<(DEPTH)/nb/nb.gyp:nb',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/starboard/starboard.gyp:starboard',
          ],
        }]
      ],
    },
    {
      'target_name': 'nb_test_deploy',
      'type': 'none',
      'dependencies': [
        'nb_test',
      ],
      'variables': {
        'executable_name': 'nb_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'reuse_allocator_benchmark',
      'type': '<(gtest_target_type)',
      'sources': [
        'reuse_allocator_benchmark.cc',
      ],
      'dependencies': [
        '<(DEPTH)/nb/nb.gyp:nb',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        'nb_copy_test_data',
      ],
    },
    {
      'target_name': 'reuse_allocator_benchmark_deploy',
      'type': 'none',
      'dependencies': [
        'reuse_allocator_benchmark',
      ],
      'variables': {
        'executable_name': 'reuse_allocator_benchmark',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'nb_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/nb/testdata/',
        ],
        'content_test_output_subdir': 'nb/testdata',
      },
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },

  ],
}
