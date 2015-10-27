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
        'condition_variable_broadcast_test.cc',
        'condition_variable_create_test.cc',
        'condition_variable_destroy_test.cc',
        'condition_variable_signal_test.cc',
        'condition_variable_wait_test.cc',
        'condition_variable_wait_timed_test.cc',
        'directory_can_open_test.cc',
        'directory_close_test.cc',
        'directory_create_test.cc',
        'directory_get_next_test.cc',
        'directory_open_test.cc',
        'double_is_finite_test.cc',
        'file_can_open_test.cc',
        'file_close_test.cc',
        'file_get_info_test.cc',
        'file_get_path_info_test.cc',
        'file_helpers.cc',
        'file_open_test.cc',
        'file_read_test.cc',
        'file_seek_test.cc',
        'file_truncate_test.cc',
        'file_write_test.cc',
        'include_all.c',
        'main.cc',
        'memory_allocate_aligned_test.cc',
        'memory_allocate_test.cc',
        'memory_compare_test.cc',
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
        'string_compare_no_case_n_test.cc',
        'string_compare_no_case_test.cc',
        'string_compare_wide_test.cc',
        'string_concat_test.cc',
        'string_concat_wide_test.cc',
        'string_copy_test.cc',
        'string_copy_wide_test.cc',
        'string_duplicate_test.cc',
        'string_format_test.cc',
        'string_format_wide_test.cc',
        'system_get_path_test.cc',
        'system_get_random_uint64_test.cc',
        'thread_create_test.cc',
        'thread_detach_test.cc',
        'thread_get_current_test.cc',
        'thread_get_id_test.cc',
        'thread_get_name_test.cc',
        'thread_helpers.cc',
        'thread_join_test.cc',
        'thread_local_value_test.cc',
        'thread_set_name_test.cc',
        'thread_sleep_test.cc',
        'thread_yield_test.cc',
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
