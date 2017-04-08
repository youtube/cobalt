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
  'targets': [
    {
      'target_name': 'platform_deploy',
      'type': 'none',
      'actions': [{
        'action_name': 'build_apk',
        'variables': {
          # TODO we want this to be determined by the *_deploy target, but
          # we can't. Instead, we may have the deploy step generate shell
          # scripts to run the app launcher and fill this in there.
          # NOTE: this becomes "libstarboard.so" inside the APK.
          'lib': '<@(PRODUCT_DIR)/lib/libcobalt.so',
          'gradle_build_dir_full_path': '<@(PRODUCT_DIR)/gradle-build',
        },
        'inputs': [
          '>(lib)'
        ],
        'outputs': [
          '<(gradle_build_dir_full_path)/app/outputs/apk/app-<(ANDROID_ABI)-debug.apk'
        ],
        'action': [
          '<(DEPTH)/starboard/android/apk/cobalt-gradle.sh',
          '--sdk', '<(ANDROID_HOME)',
          '--ndk', '<(NDK_HOME)',
          '-P', 'cobaltPlatformDeploy=true',
          '-P', 'cobaltProductDir=<@(PRODUCT_DIR)',
          '-P', 'buildDir=<(gradle_build_dir_full_path)',
          '-p', '<(DEPTH)/starboard/android/apk/',
          '--project-cache-dir', '<@(PRODUCT_DIR)/gradle-cache',
          'cobaltAssembleForDeploy_<(ANDROID_ABI)'
        ]},
      {
        'action_name': 'copy_apk',
        'inputs': [
          '<(PRODUCT_DIR)/gradle-build/app/outputs/apk/app-<(ANDROID_ABI)-debug.apk'
        ],
        'outputs': [
          '<(PRODUCT_DIR)/coat-debug.apk'
        ],
        'action': [
          # Note by doing an mv here instead of a cp, we cause
          # the above build_apk rule to be dirty next time the
          # platform_deploy target is requested. This is necessary
          # because we do not have explicit dependencies on the gradle
          # build's *.java and other input files, so we must simply
          # run it again.
          'mv',
          '<(PRODUCT_DIR)/gradle-build/app/outputs/apk/app-<(ANDROID_ABI)-debug.apk',
          '<(PRODUCT_DIR)/coat-debug.apk'
        ]
      },
  ]}]
}
