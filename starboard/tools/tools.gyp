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

# The Starboard "all" target, which includes all interesting targets for the
# Starboard project.

{
  'targets': [
    {
      'target_name': 'app_launcher_zip',
      'type': 'none',
      'variables': {
        'app_launcher_packager_path': 'app_launcher_packager.py',
        'app_launcher_zip_file': '<(PRODUCT_DIR)/app_launcher.zip',
      },
      'actions': [
        {
          'action_name': 'package_app_launcher',
          'message': 'Zipping <(app_launcher_zip_file)',
          'inputs': [
            '<!@pymod_do_main(starboard.tools.app_launcher_packager -l)',
          ],
          'outputs': [
            '<(app_launcher_zip_file)',
          ],
          'action': [
            'python2',
            '<(app_launcher_packager_path)',
            '-z',
            '<(app_launcher_zip_file)',
          ],
        },
      ],
    },
  ]
}
