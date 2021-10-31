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
    'variables': {
      'has_nb_platform': '<!pymod_do_main(starboard.build.gyp_functions file_exists <(sb_target_platform)/nb_platform.gyp)'
    },
    'nb_dependencies': [],
    'conditions': [
      ['has_nb_platform==1', {
        'nb_dependencies': [
          '<(DEPTH)/nb/<(sb_target_platform)/nb_platform.gyp:nb_platform',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'nb',
      'type': 'static_library',
      'variables': {
        'includes_starboard': 1,
      },
      'conditions': [
        ['OS=="starboard"', {
          'sources': [
            'allocator.cc',
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
            'starboard_aligned_memory_deleter.h',
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
            '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
            '<@(nb_dependencies)',
          ],
        }],
      ],
    },

  ],
}
