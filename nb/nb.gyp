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
  'targets': [
    {
      'target_name': 'nb',
      'type': 'static_library',
      'variables': {
        'includes_starboard': 1,
      },
      'conditions': [
        ['OS=="starboard" or (OS=="lb_shell" and target_arch == "ps3")', {
          'sources': [
            'allocator.h',
            'analytics/memory_tracker.cc',
            'analytics/memory_tracker.h',
            'analytics/memory_tracker_impl.cc',
            'analytics/memory_tracker_impl.h',
            'analytics/memory_tracker_helpers.cc',
            'analytics/memory_tracker_helpers.h',
            'atomic.h',
            'bit_cast.h',
            'bidirectional_fit_reuse_allocator.h',
            'bidirectional_fit_reuse_allocator.cc',
            'concurrent_map.h',
            'concurrent_ptr.h',
            'first_fit_reuse_allocator.h',
            'first_fit_reuse_allocator.cc',
            'fixed_no_free_allocator.cc',
            'fixed_no_free_allocator.h',
            'hash.cc',
            'hash.h',
            'lexical_cast.h',
            'memory_pool.h',
            'memory_scope.cc',
            'memory_scope.h',
            'move.h',
            'multipart_allocator.cc',
            'multipart_allocator.h',
            'pointer_arithmetic.h',
            'rect.h',
            'ref_counted.cc',
            'ref_counted.h',
            'reuse_allocator_base.cc',
            'reuse_allocator_base.h',
            'rewindable_vector.h',
            'starboard_memory_allocator.h',
            'scoped_ptr.h',
            'simple_thread.cc',
            'simple_thread.h',
            'simple_profiler.cc',
            'simple_profiler.h',
            'std_allocator.h',
            'string_interner.cc',
            'string_interner.h',
            'thread_collision_warner.cc',
            'thread_collision_warner.h',
            'thread_local_object.h',
            'thread_local_boolean.h',
            'thread_local_pointer.h',
          ],
          'dependencies': [
            '<(DEPTH)/starboard/starboard.gyp:starboard',
          ],
        }],
        ['target_arch == "ps4"', {
          'sources': [
            'kernel_contiguous_allocator_ps4.cc',
            'kernel_contiguous_allocator_ps4.h',
            'kernel_no_free_allocator_ps4.cc',
            'kernel_no_free_allocator_ps4.h',
          ],
        }],
      ],
    },

    {
      'target_name': 'nb_test',
      'type': '<(gtest_target_type)',
      'conditions': [
        ['OS=="starboard" or (OS=="lb_shell" and target_arch == "ps3")', {
          'sources': [
            'analytics/memory_tracker_helpers_test.cc',
            'analytics/memory_tracker_impl_test.cc',
            'analytics/memory_tracker_test.cc',
            'atomic_test.cc',
            'bidirectional_fit_reuse_allocator_test.cc',
            'concurrent_map_test.cc',
            'concurrent_ptr_test.cc',
            'first_fit_reuse_allocator_test.cc',
            'fixed_no_free_allocator_test.cc',
            'lexical_cast_test.cc',
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
            'nb',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
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
      'includes': [ '../starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'reuse_allocator_benchmark',
      'type': '<(gtest_target_type)',
      'sources': [
        'reuse_allocator_benchmark.cc',
      ],
      'dependencies': [
        'nb',
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
