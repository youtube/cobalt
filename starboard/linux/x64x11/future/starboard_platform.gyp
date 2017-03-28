# Copyright 2016 Google Inc. All Rights Reserved.
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
  'includes': [
    '../starboard_platform.gypi'
  ],
  'targets': [
    {
      'target_name': 'starboard_platform',
      'product_name': 'starboard_platform_future',
      'type': 'static_library',
      'sources': [
        '<@(starboard_platform_sources)',
        '<(DEPTH)/starboard/shared/starboard/media/codec_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/codec_util.h',
        '<(DEPTH)/starboard/shared/starboard/media/media_is_audio_supported_aac_and_opus.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_is_video_supported_h264_1080p_hfr_only.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_util.h',
        '<(DEPTH)/starboard/shared/stub/accessibility_get_display_settings.cc',
        '<(DEPTH)/starboard/shared/stub/accessibility_get_text_to_speech_settings.cc',
        '<(DEPTH)/starboard/shared/stub/cryptography_create_transformer.cc',
        '<(DEPTH)/starboard/shared/stub/cryptography_destroy_transformer.cc',
        '<(DEPTH)/starboard/shared/stub/cryptography_transform_block.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_create_egl.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_get_format.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_get_info.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_get_plane_egl.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_get_size.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_is_opaque.cc',
        '<(DEPTH)/starboard/shared/stub/decode_target_release.cc',
        '<(DEPTH)/starboard/shared/stub/image_decode.cc',
        '<(DEPTH)/starboard/shared/stub/image_is_decode_supported.cc',
      ],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<@(starboard_platform_dependencies)',
      ],
    },
  ],
}
