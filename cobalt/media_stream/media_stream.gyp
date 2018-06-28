# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'media_stream',
      'type': 'static_library',
      'sources': [
        'audio_parameters.cc',
        'audio_parameters.h',
        'media_stream.h',
        'media_stream_audio_deliverer.h',
        'media_stream_audio_sink.h',
        'media_stream_audio_source.cc',
        'media_stream_audio_source.h',
        'media_stream_audio_track.cc',
        'media_stream_audio_track.h',
        'media_stream_source.cc',
        'media_stream_source.h',
        'media_stream_track.h',
        'microphone_audio_source.cc',
        'microphone_audio_source.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ],
    },
  ],
}

