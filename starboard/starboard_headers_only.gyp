# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

# The common "starboard_headers_only" target, like the common "starboard" target
# but without the starboard_platform implementation. If a non-executable target
# depends on Starboard, but is also used by Starboard ports, the target should
# depend on this common header target. This avoids file dependency cycles for
# targets simultaneously above and below starboard. In contrast, executable
# targets such as test targets, must in some way depend on the common
# "starboard" target.

{
  'targets': [
    {
      'target_name': 'starboard_headers_only',
      'type': 'none',
      'sources': [
        'atomic.h',
        'audio_sink.h',
        'blitter.h',
        'byte_swap.h',
        'character.h',
        'condition_variable.h',
        'configuration.h',
        'configuration_constants.h',
        'cpu_features.h',
        'decode_target.h',
        'directory.h',
        'double.h',
        'drm.h',
        'egl.h',
        'event.h',
        'export.h',
        'file.h',
        'gles.h',
        'input.h',
        'key.h',
        'log.h',
        'media.h',
        'memory.h',
        'microphone.h',
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
        # Include private headers, if present.
        '<!@pymod_do_main(starboard.build.gyp_functions file_glob <(DEPTH)/starboard/private *.h)',
      ],
    },
  ],
}
