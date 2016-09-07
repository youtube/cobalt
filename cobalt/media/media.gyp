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
      'sources': [
        'can_play_type_handler.h',
        'fetcher_buffered_data_source.cc',
        'fetcher_buffered_data_source.h',
        'media_module.h',
        'media_module_stub.cc',
        'media_module_stub.h',
        'shell_video_data_allocator_common.cc',
        'shell_video_data_allocator_common.h',
        'web_media_player_factory.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/media/media.gyp:media',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/media/media.gyp:media',
      ],
      'conditions': [
        ['OS=="starboard"', {
          'sources': [
            'media_module_<(sb_media_platform).cc',
            'shell_media_platform_<(sb_media_platform).cc',
            'shell_media_platform_<(sb_media_platform).h',
          ],
          'dependencies': [
            '<(DEPTH)/nb/nb.gyp:nb',
          ],
        }],
        ['OS=="starboard" and sb_media_platform == "ps4"', {
          'sources': [
            'decoder_working_memory_allocator_impl_ps4.cc',
            'decoder_working_memory_allocator_impl_ps4.h',
            'shell_video_data_allocator_stub.cc',
            'shell_video_data_allocator_stub.h',
          ],
        }],
        ['OS=="lb_shell"', {
          'sources': [
            'media_module_<(actual_target_arch).cc',
          ],
          'include_dirs': [
            '<(lbshell_root)/src',
            '<(lbshell_root)/src/platform/<(target_arch)',
          ],
        }],
        ['OS=="lb_shell" and target_arch == "ps3"', {
          'sources': [
            'shell_media_platform_ps3.cc',
            'shell_media_platform_ps3.h',
          ],
          'dependencies': [
            '<(DEPTH)/nb/nb.gyp:nb',
          ],
        }],
      ],
    },
  ],
}
