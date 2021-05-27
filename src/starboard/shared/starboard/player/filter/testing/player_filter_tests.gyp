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
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/audio_channel_layout_mixer_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/audio_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/audio_renderer_internal_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/file_cache_reader_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/media_time_provider_impl_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/player_components_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/test_util.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/test_util.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/video_decoder_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/video_decoder_test_fixture.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/video_decoder_test_fixture.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/video_frame_cadence_pattern_generator_test.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/video_frame_rate_estimator_test.cc',
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
        '<(DEPTH)/starboard/shared/starboard/player/player.gyp:video_dmp',
        '<(DEPTH)/starboard/shared/starboard/player/player.gyp:player_copy_test_data',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
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
    {
      'target_name': 'player_filter_benchmarks',
      'type': '<(final_executable_type)',
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
        # This allows the benchmarks to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
      'sources': [
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/audio_decoder_benchmark.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/testing/test_util.cc',
        '<(DEPTH)/starboard/common/benchmark_main.cc',
      ],
      'dependencies': [
        '<@(cobalt_platform_dependencies)',
        '<(DEPTH)/starboard/shared/starboard/player/player.gyp:video_dmp',
        '<(DEPTH)/starboard/shared/starboard/player/player.gyp:player_copy_test_data',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/google_benchmark/google_benchmark.gyp:google_benchmark',
      ],
    },
    {
      'target_name': 'player_filter_benchmarks_deploy',
      'type': 'none',
      'dependencies': [
        'player_filter_benchmarks',
      ],
      'variables': {
        'executable_name': 'player_filter_benchmarks',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}
