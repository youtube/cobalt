# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
    'executable_file': '<(PRODUCT_DIR)/lib/lib<(executable_name).so',
    'deploy_executable_file': '<(target_deploy_dir)/lib/lib<(executable_name).so',

    # An absolute path is used when creating a symlink to the |executable_file|
    # since relative paths will break if they are moved or copied (before
    # Evergreen targets are deployed to a platform they are staged in a
    # different directory).
    'absolute_path_base': '<!(python -c "import os; print os.getcwd()")',
  },
  'includes': [
    '<(DEPTH)/starboard/build/collect_deploy_content.gypi',
  ],
  'actions': [
    {
      'action_name': 'deploy_executable',
      'message': 'Deploy <(absolute_path_base)/<(deploy_executable_file)',
      'inputs': [
        '<(content_deploy_stamp_file)',
        '<(executable_file)',
      ],
      'outputs': [ '<(deploy_executable_file)' ],
      'action': [
        'python',
        '<(DEPTH)/starboard/tools/port_symlink.py',
        '-f',
        '--link',
        '<(absolute_path_base)/<(executable_file)',
        '<(absolute_path_base)/<(deploy_executable_file)',
      ],
    },
  ],
}

