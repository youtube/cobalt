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
  'variables': {
    'cobalt_code': 1,
  },
  'targets': [
    {
      'target_name': 'speech',
      'type': 'static_library',
      'sources': [
        'audio_encoder_flac.cc',
        'audio_encoder_flac.h',
        'mic.h',
        'speech_recognition.cc',
        'speech_recognition.h',
        'speech_recognition_alternative.cc',
        'speech_recognition_alternative.h',
        'speech_recognition_config.h',
        'speech_recognition_error.cc',
        'speech_recognition_error.h',
        'speech_recognition_event.cc',
        'speech_recognition_event.h',
        'speech_recognition_manager.cc',
        'speech_recognition_manager.h',
        'speech_recognition_result.cc',
        'speech_recognition_result.h',
        'speech_recognition_result_list.cc',
        'speech_recognition_result_list.h',
        'speech_recognizer.cc',
        'speech_recognizer.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/third_party/flac/flac.gyp:libflac',
      ],
      'conditions': [
        ['OS=="starboard"', {
          'sources': [
            'mic_starboard.cc',
          ],
        }],
        ['OS!="starboard" and actual_target_arch in ["linux", "ps3", "win"]', {
          'sources': [
            'mic_<(actual_target_arch).cc',
          ],
        }],
      ],
    },
  ],
}
