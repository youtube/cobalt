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
  'targets': [
    {
      'target_name': 'libav.54.35.1',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [ 'libav.54.35.1' ],
      },
    },
    {
      'target_name': 'libav.56.1.0',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [ 'libav.56.1.0' ],
      },
    },
    {
      'target_name': 'ffmpeg.57.107.100',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [ 'ffmpeg.57.107.100' ],
      },
    },
  ],
}
