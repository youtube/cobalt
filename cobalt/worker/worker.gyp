# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'worker',
      'type': 'static_library',
      'sources': [
        'service_worker_container.cc',
        'service_worker_container.h',
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
