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
    'GRADLE_BUILD_TYPE': '<(cobalt_config)',
    'GRADLE_FILES_DIR': '<(PRODUCT_DIR)/gradle/<(executable_name)',
    'executable_file': '<(PRODUCT_DIR)/lib/lib<(executable_name).so',
    'apk': '<(PRODUCT_DIR)/<(executable_name).apk',
  },
  'conditions': [
    [ 'cobalt_config == "gold"', {
      'variables': {
        # Android Gradle wants a "release" build type, so use that for "gold".
        'GRADLE_BUILD_TYPE': 'release'
      }
    }]
  ],
  'dependencies': [ '<(DEPTH)/starboard/android/apk/apk.gyp:apk_sources' ],
  'includes': [ '<(DEPTH)/starboard/build/collect_deploy_content.gypi' ],
  'actions': [
    {
      'action_name': 'build_apk',
      'inputs': [
        '<(executable_file)',
        '<(PRODUCT_DIR)/gradle/apk_sources.stamp',
        '<(content_deploy_stamp_file)',
      ],
      'outputs': [ '<(apk)' ],
      'action': [
        '<(DEPTH)/starboard/android/apk/cobalt-gradle.sh',
        '--sdk', '<(ANDROID_HOME)',
        '--ndk', '<(NDK_HOME)',
        '-p', '<(DEPTH)/starboard/android/apk/',
        '--project-cache-dir', '<(GRADLE_FILES_DIR)/cache',
        '-P', 'cobaltBuildAbi=<(ANDROID_ABI)',
        '-P', 'cobaltDeployApk=<(apk)',
        '-P', 'cobaltContentDir=<(content_deploy_dir)',
        '-P', 'cobaltGradleDir=<(GRADLE_FILES_DIR)',
        '-P', 'cobaltProductDir=<(PRODUCT_DIR)',
        '-P', 'cobaltTarget=<(executable_name)',
        'assembleCobalt_<(GRADLE_BUILD_TYPE)',
      ],
      'message': 'Build APK: <(apk)',
    },
    {
      # Clean the gradle directory to conserve space after we have the APK
      'action_name': 'delete_gradle_dir',
      'inputs': [ '<(apk)' ],
      'outputs': [ '<(GRADLE_FILES_DIR).deleted.stamp' ],
      'action': [
        'sh', '-c',
        'rm -rf <(GRADLE_FILES_DIR) && touch <(GRADLE_FILES_DIR).deleted.stamp',
      ],
      'message': 'Cleanup Gradle: <(executable_name)',
    },
  ],
}
