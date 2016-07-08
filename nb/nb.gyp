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
      'sources': [
        'allocator.h',
        'allocator_decorator.cc',
        'allocator_decorator.h',
        'fixed_no_free_allocator.cc',
        'fixed_no_free_allocator.h',
        'memory_pool.cc',
        'memory_pool.h',
        'move.h',
        'pointer_arithmetic.h',
        'rect.h',
        'ref_counted.cc',
        'ref_counted.h',
        'reuse_allocator.cc',
        'reuse_allocator.h',
        'scoped_ptr.h',
        'thread_collision_warner.cc',
        'thread_collision_warner.h',
      ],

      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],

      'conditions': [
        ['target_arch == "ps4"', {
          'sources': [
            'kernel_no_free_allocator_ps4.cc',
            'kernel_no_free_allocator_ps4.h',
          ],
        }],
      ],
    },

    {
      'target_name': 'nb_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'fixed_no_free_allocator_test.cc',
        'reuse_allocator_test.cc',
        'run_all_unittests.cc',
      ],
      'dependencies': [
        'nb',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
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
    },

    {
      'target_name': 'reuse_allocator_benchmark',
      'type': '<(gtest_target_type)',
      'sources': [
        'reuse_allocator_benchmark.cc',
      ],
      'dependencies': [
        'nb',
      ],
      'actions': [
        {
          'action_name': 'reuse_allocator_benchmark_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/nb/testdata/',
            ],
            'output_dir': 'nb/testdata',
          },
          'includes': ['../starboard/build/copy_test_data.gypi'],
        }
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
  ],
}
