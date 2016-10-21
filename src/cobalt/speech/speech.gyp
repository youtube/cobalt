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
        'chunked_byte_buffer.cc',
        'chunked_byte_buffer.h',
        'google_streaming_api.pb.cc',
        'google_streaming_api.pb.h',
        'google_streaming_api.pb.proto',
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
      'include_dirs': [
        # Get protobuf headers from the chromium tree.
        '<(DEPTH)/third_party/protobuf/src',
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
    {
      'target_name': 'speech_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'chunked_byte_buffer_unittest.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },

    {
      'target_name': 'speech_test_deploy',
      'type': 'none',
      'dependencies': [
        'speech_test',
      ],
      'variables': {
        'executable_name': 'speech_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}
