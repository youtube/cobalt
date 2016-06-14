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
      'target_name': 'account',
      'type': 'static_library',
      'sources': [
        'account_event.h',
        'account_manager.h',
      ],
      'conditions': [
        ['OS=="starboard"', {
          'sources': [
            'starboard/account_manager.cc',
          ],
        }, {
          'sources': [
            '<(actual_target_arch)/account_manager.cc',
          ],
        }],
        ['OS!="starboard" and target_arch=="ps3"', {
          'sources': [
            'ps3/account_manager.h',
            'ps3/psn_state_machine.cc',
            'ps3/psn_state_machine.h',
          ],
        }],
        ['OS!="starboard" and actual_target_arch=="win"', {
          'sources': [
            'account_manager_stub.cc',
            'account_manager_stub.h',
          ],
        }],
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
      ],
    },
  ],
}
