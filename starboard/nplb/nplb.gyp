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

# NPLB is "No Platform Left Behind," the certification test suite for Starboard
# implementations.

{
  'targets': [
    {
      'target_name': 'nplb',
      'type': '<(gtest_target_type)',
      'sources': [
        'main.cc',
        'memory_allocate_aligned_test.cc',
        'memory_allocate_test.cc',
        'memory_copy_test.cc',
        'memory_free_aligned_test.cc',
        'memory_free_test.cc',
        'memory_move_test.cc',
        'memory_reallocate_test.cc',
        'memory_set_test.cc',
        'mutex_acquire_test.cc',
        'mutex_acquire_try_test.cc',
        'mutex_create_test.cc',
        'mutex_destroy_test.cc',
        'mutex_release_test.cc',
        'system_get_path_test.cc',
        'time_explode_test.cc',
        'time_get_monotonic_now_test.cc',
        'time_get_now_test.cc',
        'time_implode_test.cc',
        'time_zone_get_current_test.cc',
        'time_zone_get_dst_name_test.cc',
        'time_zone_get_name_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
  ],
}
