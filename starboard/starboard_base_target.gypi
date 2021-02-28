# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

# This file is included in the target defaults from
# cobalt/build/config/base.gypi
{
  'defines': [
    # The canonical way to detect Starboard is #if defined(STARBOARD). This
    # should be defined both when building Starboard itself, and when building
    # any other source file in a Starboard-based project.
    'STARBOARD',
  ],
  'conditions': [
    ['gl_type == "none"', {
      'defines': [
        'SB_GYP_GL_TYPE_IS_NONE=1',
      ],
    }, {
      'defines': [
        'SB_GYP_GL_TYPE_IS_NONE=0',
      ],
    }],
    ['abort_on_allocation_failure==1', {
      'defines': [
        'SB_ABORT_ON_ALLOCATION_FAILURE',
      ],
    }],
    ['sb_allows_memory_tracking==1', {
      'defines': [
        'STARBOARD_ALLOWS_MEMORY_TRACKING',
      ],
    }],
    ['sb_enable_lib==1', {
      'defines': [
        'SB_IS_LIBRARY=1',
      ],
    }],
    ['sb_evergreen==1', {
      'defines': [
        'SB_IS_EVERGREEN=1',
      ],
    }],
    ['sb_evergreen_compatible==1', {
      'defines': [
        'SB_IS_EVERGREEN_COMPATIBLE=1',
      ],
    }],
    ['sb_evergreen_compatible_libunwind==1', {
      'defines': [
        'SB_IS_EVERGREEN_COMPATIBLE_LIBUNWIND=1',
      ],
    }],
    ['sb_evergreen_compatible_lite==1', {
      'defines': [
        'SB_IS_EVERGREEN_COMPATIBLE_LITE=1',
      ],
    }],
    ['starboard_path == ""', {
      'defines': [
        # There doesn't appear to be any way to use the C preprocessor to do
        # string concatenation with the / character. This prevents us from using
        # the preprocessor to assemble an include file path, so we have to do
        # the concatenation here in GYP.
        # http://stackoverflow.com/questions/29601786/c-preprocessor-building-a-path-string
        'STARBOARD_ATOMIC_INCLUDE="starboard/<(target_arch)/atomic_public.h"',
        'STARBOARD_CONFIGURATION_INCLUDE="starboard/<(target_arch)/configuration_public.h"',
        'STARBOARD_THREAD_TYPES_INCLUDE="starboard/<(target_arch)/thread_types_public.h"',
      ],
    }, {
      'defines': [
        'STARBOARD_ATOMIC_INCLUDE="<(starboard_path)/atomic_public.h"',
        'STARBOARD_CONFIGURATION_INCLUDE="<(starboard_path)/configuration_public.h"',
        'STARBOARD_THREAD_TYPES_INCLUDE="<(starboard_path)/thread_types_public.h"',
      ],
    }],
  ],
}
