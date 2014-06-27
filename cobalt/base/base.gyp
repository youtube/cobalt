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
    # Fake target that captures header files for the sake of Visual Studio
    # project generation. This will eventually turn into a static library
    # once .cc files are added.
    {
      'target_name': 'base_lib',
      'type': 'none',
      'sources': [
        'compiler.h',
      ],
    },
  ],
}
