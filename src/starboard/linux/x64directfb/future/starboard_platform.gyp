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
      'product_name': 'starboard_platform_future',
      'type': 'static_library',
      'sources': [],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/common/common.gyp:common',
        '<(DEPTH)/starboard/linux/x64directfb/starboard_platform.gyp:starboard_platform',
        '<(DEPTH)/third_party/dlmalloc/dlmalloc.gyp:dlmalloc',
        '<(DEPTH)/third_party/libevent/libevent.gyp:libevent',
      ],
    },
  ],
}
