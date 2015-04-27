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
  'targets': [
    {
      'target_name': 'media',
      'type': 'static_library',
      'sources': [
        'media_module.h',
        'media_module_<(actual_target_arch).cc',
      ],
      'conditions': [
        ['target_arch == "ps3"', {
          # TODO(***REMOVED***) : Find a better place to put lbshell files and add
          #                   support for other platforms like Linux.
          'sources': [
            'shell_media_platform_ps3.cc',
            'shell_media_platform_ps3.h',
            'shell_video_data_allocator_ps3.cc',
            'shell_video_data_allocator_ps3.h',
            '<(lbshell_root)/src/lb_memory_pool.cc',
            '<(lbshell_root)/src/lb_memory_pool.h',
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
          ],
          'include_dirs': [
            '<(lbshell_root)/src',
            '<(lbshell_root)/src/platform/<(target_arch)',
          ],
          'dependencies': [
            '<(DEPTH)/media/media.gyp:media',
            # For resampler
            '<(lbshell_root)/build/platforms/ps3.gyp:spurs_tasks',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/media/media.gyp:media',
          ],
        }],
      ],
    },
  ],
}
