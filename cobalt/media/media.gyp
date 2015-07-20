# Copyright 2014 Google Inc. All Rights Reserved.
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
  'variables': {
    'cobalt_code': 1,
  },
  'targets': [
    {
      'target_name': 'media',
      'type': 'static_library',
      'include_dirs': [
        '<(lbshell_root)/src',
        '<(lbshell_root)/src/platform/<(target_arch)',
      ],
      'sources': [
        '<(lbshell_root)/src/lb_memory_pool.cc',
        '<(lbshell_root)/src/lb_memory_pool.h',
        'fetcher_buffered_data_source.cc',
        'fetcher_buffered_data_source.h',
        'media_module.cc',
        'media_module.h',
        'media_module_<(actual_target_arch).cc',
        'shell_video_data_allocator_common.cc',
        'shell_video_data_allocator_common.h',
        'web_media_player_factory.h',
      ],
      'dependencies': [
        '<(DEPTH)/media/media.gyp:media',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/media/media.gyp:media',
      ],
      'conditions': [
        ['target_arch == "linux"', {
          'sources': [
            '<(lbshell_root)/src/platform/linux/lb_shell/lb_ffmpeg.cc',
            '<(lbshell_root)/src/platform/linux/lb_shell/lb_ffmpeg.h',
            '<(lbshell_root)/src/platform/linux/lb_shell/lb_pulse_audio.cc',
            '<(lbshell_root)/src/platform/linux/lb_shell/lb_pulse_audio.h',
            '<(lbshell_root)/src/platform/linux/lb_shell/shell_audio_streamer_linux.cc',
            '<(lbshell_root)/src/platform/linux/lb_shell/shell_audio_streamer_linux.h',
            '<(lbshell_root)/src/platform/linux/lb_shell/shell_raw_audio_decoder_linux.cc',
            '<(lbshell_root)/src/platform/linux/lb_shell/shell_raw_video_decoder_linux.cc',
            'shell_media_platform_linux.cc',
            'shell_media_platform_linux.h',
          ],
        }],
        ['target_arch == "ps3"', {
          'sources': [
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_audio_resampler_ps3.cc',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_audio_resampler_ps3.h',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_main_memory_decoder_buffer_ps3.cc',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_main_memory_decoder_buffer_ps3.h',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_spurs.cc',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_spurs.h',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_vdec_helper_ps3.cc',
            '<(lbshell_root)/src/platform/ps3/lb_shell/lb_vdec_helper_ps3.h',
            '<(lbshell_root)/src/platform/ps3/lb_shell/shell_audio_streamer_ps3.cc',
            '<(lbshell_root)/src/platform/ps3/lb_shell/shell_audio_streamer_ps3.h',
            '<(lbshell_root)/src/platform/ps3/lb_shell/shell_raw_video_decoder_ps3.cc',
            '<(lbshell_root)/src/platform/ps3/lb_shell/shell_raw_video_decoder_ps3.h',
            'shell_media_platform_ps3.cc',
            'shell_media_platform_ps3.h',
          ],
          'dependencies': [
            # For resampler
            '<(lbshell_root)/build/platforms/ps3.gyp:spurs_tasks',
          ],
        }],
      ],
    },
  ],
}
