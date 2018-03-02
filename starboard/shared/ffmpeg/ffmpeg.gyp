# Copyright 2018 Google Inc. All Rights Reserved.
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
  'target_defaults': {
      'defines': [
        'STARBOARD_IMPLEMENTATION',
      ],
  },
  'variables': {
    'ffmpeg_specialization_sources': [
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl.h',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_common.h',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_video_decoder_impl.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_video_decoder_impl.h',
    ],
    'ffmpeg_dispatch_sources': [
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_audio_decoder.h',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_dispatch.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_dispatch.h',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_video_decoder.h',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_video_decoder_internal.cc',
    ],
  },
  'targets': [
    {
      'target_name': 'libav.54.35.1',
      'type': '<(library)',
      'sources': [ '<@(ffmpeg_specialization_sources)' ],
      'dependencies': [
        '<(DEPTH)/third_party/ffmpeg_includes/ffmpeg_includes.gyp:libav.54.35.1',
      ],
    },
    {
      'target_name': 'libav.56.1.0',
      'type': '<(library)',
      'sources': [ '<@(ffmpeg_specialization_sources)' ],
      'dependencies': [
        '<(DEPTH)/third_party/ffmpeg_includes/ffmpeg_includes.gyp:libav.56.1.0',
      ],
    },
    {
      'target_name': 'ffmpeg.57.107.100',
      'type': '<(library)',
      'sources': [ '<@(ffmpeg_specialization_sources)' ],
      'dependencies': [
        '<(DEPTH)/third_party/ffmpeg_includes/ffmpeg_includes.gyp:ffmpeg.57.107.100',
      ],
    },
    {
      'target_name': 'ffmpeg_dynamic_load',
      'type': '<(library)',
      'sources': [
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_dynamic_load_audio_decoder_impl.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_dynamic_load_dispatch_impl.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_dynamic_load_video_decoder_impl.cc',
        '<@(ffmpeg_dispatch_sources)',
      ],
      'dependencies': [
        'ffmpeg.57.107.100',
        'libav.54.35.1',
        'libav.56.1.0',
      ],
    },
    {
      'target_name': 'ffmpeg_linked',
      'type': '<(library)',
      'sources': [
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_linked_audio_decoder_impl.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_linked_dispatch_impl.cc',
        '<(DEPTH)/starboard/shared/ffmpeg/ffmpeg_linked_video_decoder_impl.cc',
        '<@(ffmpeg_dispatch_sources)',
        '<@(ffmpeg_specialization_sources)',
      ],
    },
  ],
}
