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
  'targets': [
    {
      'target_name': 'speech',
      'type': 'static_library',
      'sources': [
        'chunked_byte_buffer.cc',
        'chunked_byte_buffer.h',
        'endpointer/endpointer.cc',
        'endpointer/endpointer.h',
        'endpointer/energy_endpointer.cc',
        'endpointer/energy_endpointer.h',
        'endpointer/energy_endpointer_params.cc',
        'endpointer/energy_endpointer_params.h',
      ],
    },
    {
      'target_name': 'speech_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'chunked_byte_buffer_unittest.cc',
        'endpointer/endpointer_unittest.cc',
      ],
      'dependencies': [
        'speech',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/media/media2.gyp:media2',
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
      'includes': [ '../../../starboard/build/deploy.gypi' ],
    },
  ],
}
