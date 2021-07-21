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
      'target_name': 'player',
      'type': 'static_library',
      'sources': [
        '<@(common_player_sources)',
      ],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
      'includes': ['<(DEPTH)/starboard/shared/starboard/player/common_player_sources.gypi'],
    },
    {
      'target_name': 'video_dmp',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/starboard/shared/starboard/player/file_cache_reader.cc',
        '<(DEPTH)/starboard/shared/starboard/player/file_cache_reader.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
      ],
      'defines': [
        # This allows the tests to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
    },
    {
      'target_name': 'player_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<!@pymod_do_main(starboard.build.gyp_functions file_glob_sub <(DEPTH)/starboard/shared/starboard/player/testdata *.dmp.sha1 dmp.sha1 dmp)'
        ],
        'content_test_output_subdir': 'starboard/shared/starboard/player/testdata',
      },
      'actions' : [
        {
          'variables': {
            'sha_dir': '<(DEPTH)/starboard/shared/starboard/player/testdata',
            # GYP will remove an argument to an action that looks like a duplicate, so
            # we change it semantically here.
            'out_dir': '<(DEPTH)/starboard/../starboard/shared/starboard/player/testdata',
          },
          'action_name': 'player_download_test_data',
          'action': [
            'python2',
            '<(DEPTH)/tools/download_from_gcs.py',
            '--bucket', 'cobalt-static-storage',
            '--sha1', '<(sha_dir)',
            '--output', '<(out_dir)',
          ],
          'inputs': [
            '<!@pymod_do_main(starboard.build.gyp_functions file_glob <(sha_dir) *.dmp.sha1)',
          ],
          'outputs': [
            '<!@pymod_do_main(starboard.build.gyp_functions file_glob_sub <(DEPTH)/starboard/shared/starboard/player/testdata *.dmp.sha1 dmp.sha1 dmp)',
          ],
        },
      ],
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },
  ],
}
