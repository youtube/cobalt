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
      'msvs_disabled_warnings': [
        # Dereferencing NULL pointer in generated file
        # google_streaming_api.pb.cc.
        6011,
      ],
      'sources': [
        'audio_encoder_flac.cc',
        'audio_encoder_flac.h',
        'endpointer_delegate.cc',
        'endpointer_delegate.h',
        'google_streaming_api.pb.cc',
        'google_streaming_api.pb.h',
        'google_streaming_api.pb.proto',
        'microphone.h',
        'microphone_manager.cc',
        'microphone_manager.h',
        'speech_configuration.h',
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
        '<(DEPTH)/content/browser/speech/speech.gyp:speech',
        '<(DEPTH)/third_party/flac/flac.gyp:libflac',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
      ],
      'include_dirs': [
        # Get protobuf headers from the chromium tree.
        '<(DEPTH)/third_party/protobuf/src',
      ],
      'conditions': [
        ['OS=="starboard"', {
          'sources': [
            'microphone_starboard.cc',
            'microphone_starboard.h',
          ],
        }],
        ['OS=="starboard" and enable_fake_microphone == 1', {
          'sources': [
            'microphone_fake.cc',
            'microphone_fake.h',
            'url_fetcher_fake.cc',
            'url_fetcher_fake.h',
          ],
          'defines': [
            'ENABLE_FAKE_MICROPHONE',
          ],
          'direct_dependent_settings': {
            'defines': [ 'ENABLE_FAKE_MICROPHONE', ],
          },
        }],
        ['cobalt_copy_test_data == 1', {
          'actions': [
            {
              'action_name': 'speech_copy_test_data',
              'variables': {
                'input_files': [
                  '<(DEPTH)/cobalt/speech/testdata/',
                ],
                'output_dir': 'cobalt/speech/testdata/',
              },
              'includes': [ '../build/copy_test_data.gypi' ],
            },
          ],
        }

        ],
      ],
    },
  ],
}
