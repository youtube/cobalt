# Copyright 2017 Google Inc. All Rights Reserved.
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
      'target_name': 'filter_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        '<(DEPTH)/starboard/common/test_main.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/audio_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_internal_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/fake_graphics_context_provider.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/fake_graphics_context_provider.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/media_time_provider_impl_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/video_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_reader.h',
      ],
      'defines': [
        # This allows the tests to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'actions': [
        {
          'action_name': 'copy_filter_tests_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/starboard/shared/starboard/player/testdata',
            ],
            'output_dir': '/starboard/shared/starboard/player',
          },
          'includes': ['../../../../build/copy_test_data.gypi'],
        }
      ],
    },
    {
      'target_name': 'filter_tests_deploy',
      'type': 'none',
      'dependencies': [
        'filter_tests',
      ],
      'variables': {
        'executable_name': 'filter_tests',
      },
      'includes': [ '../../../../build/deploy.gypi' ],
    },
  ],
}
