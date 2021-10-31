# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'audio_dmp_player',
      'type': '<(final_executable_type)',
      'sources': [
        '<(DEPTH)/starboard/shared/starboard/player/file_cache_reader.cc',
        '<(DEPTH)/starboard/shared/starboard/player/file_cache_reader.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.h',
        'audio_dmp_player.cc',
      ],
      'defines': [
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/shared/starboard/player/player.gyp:player_copy_test_data',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
    {
      'target_name': 'audio_dmp_player_deploy',
      'type': 'none',
      'dependencies': [
        'audio_dmp_player',
      ],
      'variables': {
        'executable_name': 'audio_dmp_player',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
