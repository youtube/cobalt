# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'media_util',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/starboard/shared/starboard/media/avc_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/avc_util.h',
        '<(DEPTH)/starboard/shared/starboard/media/codec_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/codec_util.h',
        '<(DEPTH)/starboard/shared/starboard/media/media_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_util.h',
        '<(DEPTH)/starboard/shared/starboard/media/video_capabilities.cc',
        '<(DEPTH)/starboard/shared/starboard/media/video_capabilities.h',
        '<(DEPTH)/starboard/shared/starboard/media/vp9_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/vp9_util.h',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
      ],
      'defines': [
        # This allows the tests to include internal only header files.
        'STARBOARD_IMPLEMENTATION',
      ],
    },
  ],
}
