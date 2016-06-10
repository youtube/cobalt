# Copyright 2016 Google Inc. All Rights Reserved.
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
      'target_name': 'starboard_platform',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/starboard/stub/character_is_alphanumeric.cc',
        '<(DEPTH)/starboard/stub/character_is_digit.cc',
        '<(DEPTH)/starboard/stub/character_is_hex_digit.cc',
        '<(DEPTH)/starboard/stub/character_is_space.cc',
        '<(DEPTH)/starboard/stub/character_is_upper.cc',
        '<(DEPTH)/starboard/stub/character_to_lower.cc',
        '<(DEPTH)/starboard/stub/character_to_upper.cc',
        '<(DEPTH)/starboard/stub/double_absolute.cc',
        '<(DEPTH)/starboard/stub/double_exponent.cc',
        '<(DEPTH)/starboard/stub/double_floor.cc',
        '<(DEPTH)/starboard/stub/double_is_finite.cc',
        '<(DEPTH)/starboard/stub/double_is_nan.cc',
        '<(DEPTH)/starboard/stub/log.cc',
        '<(DEPTH)/starboard/stub/log_flush.cc',
        '<(DEPTH)/starboard/stub/log_format.cc',
        '<(DEPTH)/starboard/stub/log_is_tty.cc',
        '<(DEPTH)/starboard/stub/log_raw.cc',
        '<(DEPTH)/starboard/stub/string_compare.cc',
        '<(DEPTH)/starboard/stub/string_compare_all.cc',
        '<(DEPTH)/starboard/stub/string_find_character.cc',
        '<(DEPTH)/starboard/stub/string_find_last_character.cc',
        '<(DEPTH)/starboard/stub/string_find_string.cc',
        '<(DEPTH)/starboard/stub/string_get_length.cc',
        '<(DEPTH)/starboard/stub/string_get_length_wide.cc',
        '<(DEPTH)/starboard/stub/string_parse_signed_integer.cc',
        '<(DEPTH)/starboard/stub/string_parse_uint64.cc',
        '<(DEPTH)/starboard/stub/string_parse_unsigned_integer.cc',
        '<(DEPTH)/starboard/stub/string_scan.cc',
        '<(DEPTH)/starboard/stub/time_get_monotonic_now.cc',
        '<(DEPTH)/starboard/stub/time_get_now.cc',
        '<(DEPTH)/starboard/stub/time_zone_get_current.cc',
        '<(DEPTH)/starboard/stub/time_zone_get_dst_name.cc',
        '<(DEPTH)/starboard/stub/time_zone_get_name.cc',
      ],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
    },
  ],
}
