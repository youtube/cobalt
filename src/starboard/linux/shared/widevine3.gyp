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
  'includes': [
    '<(DEPTH)/starboard/shared/widevine/widevine3.gypi',
  ],
  'variables': {
    'platform_oem_sources': [
      '<(DEPTH)/starboard/keyboxes/linux/linux.h',
      '<(DEPTH)/starboard/keyboxes/linux/linux_client.c',
      '<(DEPTH)/starboard/linux/shared/wv_keybox_linux.cc',
    ],
  },
  'target_defaults': {
    'defines': [
      'COBALT_WIDEVINE_KEYBOX_INCLUDE="<(DEPTH)/starboard/keyboxes/widevine_settings_linux.h"',
    ],
  },
}
