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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'media_capture',
      'type': 'static_library',
      'sources': [
        'media_devices.cc',
        'media_devices.h',
        'media_device_info.cc',
        'media_device_info.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ],
      'export_dependent_settings': [
        # Additionally, ensure that the include directories for generated
        # headers are put on the include directories for targets that depend
        # on this one.
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ]
    },
  ],
}
