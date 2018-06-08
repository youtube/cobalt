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
  'targets': [
    {
      # Target that represents the JavaScript engine implementation and an
      # interface to create a new engine instance.
      'target_name': 'engine',
      'type': 'static_library',
      'sources': [
        'javascript_engine.h',
      ],
      'conditions': [
        [ 'javascript_engine == "mozjs-45"', { 'dependencies': ['mozjs-45/mozjs-45.gyp:engine', ], }, ],
        [ 'javascript_engine == "v8"', { 'dependencies': ['v8c/v8c.gyp:engine', ], }, ],
      ],
    },
    {
      'target_name': 'engine_shell',
      'type': 'none',
      'conditions': [
        [ 'javascript_engine == "mozjs-45"', { 'dependencies': ['mozjs-45/mozjs-45.gyp:mozjs-45', ], }, ],
        [ 'javascript_engine == "v8"', { 'dependencies': ['v8c/v8c.gyp:v8c', ], }, ],
      ],
    },
  ],
}
