# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'player_filter_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        '<(DEPTH)/starboard/common/test_main.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/adaptive_audio_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/audio_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/audio_renderer_internal_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/media_time_provider_impl_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/video_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.h',
        '<(DEPTH)/starboard/testing/fake_graphics_context_provider.cc',
        '<(DEPTH)/starboard/testing/fake_graphics_context_provider.h',
      ],
      'conditions': [
        ['gl_type != "none"', {
          'dependencies': [
            # This is needed because VideoDecoderTest depends on
            # FakeGraphicsContextProvider which depends on EGL and GLES.
            '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
          ],
        }],
      ],
      'defines': [
        # This allows the tests to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<@(cobalt_platform_dependencies)',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'player_filter_tests_copy_test_data',
      ],
    },
    {
      'target_name': 'player_filter_tests_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': ['<!@(python <(DEPTH)/starboard/build/list_dmp_files.py "starboard/shared/starboard/player/testdata")'],
        'content_test_output_subdir': 'starboard/shared/starboard/player/testdata',
        'depot_tools_path': ['<!@(python <(DEPTH)/build/find_depot_tools_escaped.py)'],
      },
      'actions' : [
        {
          # This action requires depot_tools to be in path
          # (https://cobalt.googlesource.com/depot_tools).
          'action_name': 'player_filter_tests_download_test_data',
          'action': [ 'python',
                      '<(depot_tools_path)/download_from_google_storage.py',
                      '--no_resume',
                      '--no_auth',
                      '--num_threads', '8',
                      '--bucket', 'cobalt-static-storage',
                      '-d', '<(DEPTH)/starboard/shared/starboard/player/testdata',
          ],
          'inputs': [],
          'outputs': ['<!@(python <(DEPTH)/starboard/build/list_dmp_files.py "starboard/shared/starboard/player/testdata")'],
        },
      ],
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },
    {
      'target_name': 'player_filter_tests_deploy',
      'type': 'none',
      'dependencies': [
        'player_filter_tests',
      ],
      'variables': {
        'executable_name': 'player_filter_tests',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
