# Copyright 2021 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'targets': [
    {
      'target_name': 'one_app_only_sandbox',
      'type': '<(final_executable_type)',
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/starboard/loader_app/app_key.gyp:app_key',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
      'sources': [
        'one_app_only_sandbox.cc',
      ],
    },
    {
      'target_name': 'one_app_only_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'one_app_only_sandbox',
      ],
      'variables': {
        'executable_name': 'one_app_only_sandbox',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ]
}
