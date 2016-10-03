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

# The common "starboard" target. Any target that depends on Starboard should
# depend on this common target, and not any of the specific "starboard_platform"
# targets.

{
  'targets': [
    {
      'target_name': 'starboard',
      'type': 'none',
      'sources': [
        'atomic.h',
        'audio_sink.h',
        'blitter.h',
        'byte_swap.h',
        'character.h',
        'condition_variable.h',
        'configuration.h',
        'directory.h',
        'double.h',
        'drm.h',
        'event.h',
        'export.h',
        'file.h',
        'input.h',
        'key.h',
        'log.h',
        'media.h',
        'memory.h',
        'mutex.h',
        'once.h',
        'player.h',
        'queue.h',
        'socket.h',
        'socket_waiter.h',
        'spin_lock.h',
        'storage.h',
        'string.h',
        'system.h',
        'thread.h',
        'thread_types.h',
        'time.h',
        'time_zone.h',
        'types.h',
        'user.h',
        'window.h',
      ],
      'conditions': [
        ['starboard_path == ""', {
          # TODO: Make starboard_path required. This legacy condition is only
          # here to support semi-starboard platforms while they still exist.
          'dependencies': [
            '<(DEPTH)/starboard/<(target_arch)/starboard_platform.gyp:starboard_platform',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/starboard/<(target_arch)/starboard_platform.gyp:starboard_platform',
          ],
        }, {
          'dependencies': [
            '<(DEPTH)/<(starboard_path)/starboard_platform.gyp:starboard_platform',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/<(starboard_path)/starboard_platform.gyp:starboard_platform',
          ],
        }],
        ['final_executable_type=="shared_library"', {
          'all_dependent_settings': {
            'target_conditions': [
              ['_type=="executable" and _toolset=="target"', {
                'sources': [
                  '<(DEPTH)/starboard/shared/starboard/shared_main_adapter.cc',
                ],
              }],
            ],
          },
        }],
      ],
    },
  ],
}
