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

# The "common" target contains facilities provided by Starboard that are common
# to all platforms.

{
  'targets': [
    {
      'target_name': 'common',
      'type': 'static_library',
      'variables': {
        'includes_starboard': 1,
      },
      'sources': [
        'common.cc',
        'flat_map.h',
        'memory.cc',
        'move.h',
        'new.cc',
        'optional.cc',
        'ref_counted.cc',
        'ref_counted.h',
        'reset_and_return.h',
        'rwlock.h',
        'semaphore.h',
        'scoped_ptr.h',
        'state_machine.cc',
        'state_machine.h',
        'thread_collision_warner.cc',
        'thread_collision_warner.h',
        'thread.cc',
        'thread.h',
      ],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
      'conditions': [
        ['custom_media_session_client==0', {
          'sources': [
            '<(DEPTH)/starboard/shared/media_session/stub_playback_state.cc',
           ],
        }],
      ]
    },
  ],
}
