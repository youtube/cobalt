# Copyright 2015 Google Inc. All Rights Reserved.
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
      'target_name': 'network',
      'type': 'static_library',
      'sources': [
        'network_delegate.cc',
        'network_delegate.h',
        'network_module.cc',
        'network_module.h',
        '<(actual_target_arch)/network_system.cc',
        'network_system.h',
        'url_request_context.cc',
        'url_request_context.h',
        'url_request_context_getter.cc',
        'url_request_context_getter.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/net/net.gyp:net',
      ],

    },
  ],
}
