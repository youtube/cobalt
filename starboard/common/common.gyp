# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
        'byte_swap.h',
        'common.cc',
        'condition_variable.cc',
        'condition_variable.h',
        'configuration_defaults.cc',
        'configuration_defaults.h',
        'file.cc',
        'file.h',
        'flat_map.h',
        'locked_ptr.h',
        'log.cc',
        'log.h',
        'media.cc',
        'media.h',
        'move.h',
        'murmurhash2.cc',
        'murmurhash2.h',
        'mutex.cc',
        'mutex.h',
        'new.cc',
        'optional.cc',
        'optional.h',
        'queue.h',
        'recursive_mutex.cc',
        'recursive_mutex.h',
        'ref_counted.cc',
        'ref_counted.h',
        'reset_and_return.h',
        'rwlock.cc',
        'rwlock.h',
        'scoped_ptr.h',
        'semaphore.cc',
        'semaphore.h',
        'socket.cc',
        'socket.h',
        'spin_lock.cc',
        'spin_lock.h',
        'state_machine.cc',
        'state_machine.h',
        'storage.cc',
        'storage.h',
        'string.h',
        'thread_collision_warner.cc',
        'thread_collision_warner.h',
        'thread.cc',
        'thread.h',

        'experimental/concurrency_debug.cc',
        'experimental/concurrency_debug.h',

        '<(DEPTH)/starboard/shared/media_session/playback_state.cc',
        '<(DEPTH)/starboard/shared/media_session/playback_state.h',
      ],
    },
  ],
}
