# Copyright 2017 Google Inc. All Rights Reserved.
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
    'enable_map_to_mesh%': 1,
    'enable_key_statuses_callback%': '<!(test -e ../../third_party/cdm/cdm/include/content_decryption_module.h && grep -q GetKeys ../../third_party/cdm/cdm/include/content_decryption_module.h && echo 1 || echo 0)',
  },
  'target_defaults': {
    'defines': [
      'ENABLE_KEY_STATUSES_CALLBACK=<(enable_key_statuses_callback)',
    ],
  },

  'includes': [
    '../../shared/cobalt/configuration.gypi',
  ],
}
