# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
    'deploy_executable_file': '<(target_deploy_dir)/lib/lib<(executable_name).so',
    'executable_file': '<(PRODUCT_DIR)/lib/lib<(executable_name).so',
  },
  'includes': [ '<(DEPTH)/starboard/build/collect_deploy_content.gypi' ],
  'actions': [
    {
      'action_name': 'deploy_executable',
      'message': 'Strip executable: <(deploy_executable_file)',
      'inputs': [
        '<(executable_file)',
        '<(content_deploy_stamp_file)',
      ],
      'outputs': [ '<(deploy_executable_file)' ],
      'action': [
        'strip',
        '-o', '<(deploy_executable_file)',
        '<(executable_file)',
      ],
    },
  ],
}
