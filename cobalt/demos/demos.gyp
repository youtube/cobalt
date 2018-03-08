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
  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'copy_demos',
      'type': 'none',
      'variables': {
        'content_test_input_files': [ '<(DEPTH)/cobalt/demos/content/' ],
        'content_test_output_subdir': 'cobalt/demos',
      },
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },
  ],
}
