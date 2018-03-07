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
  'targets': [
    {
      # This target generates a stamp file that the platform deploy action can
      # depend on to re-run the Gradle build only when any file that's checked
      # into git within the 'apk' directory changes.
      'target_name': 'apk_sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'apk_sources',
          'inputs': [ '<!@(git -C <(DEPTH)/starboard/android/apk ls-files)' ],
          'outputs': [ '<(PRODUCT_DIR)/gradle/apk_sources.stamp' ],
          'action': [ 'touch', '<@(_outputs)' ],
        }
      ]
    }
  ]
}
