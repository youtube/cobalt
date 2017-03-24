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
          'java_files': [
            '../apk/app/src/main/java/foo/cobalt/AlertDialogPlatformError.java',
            '../apk/app/src/main/java/foo/cobalt/CobaltActivity.java',
            '../apk/app/src/main/java/foo/cobalt/CobaltHttpHelper.java',
            '../apk/app/src/main/java/foo/cobalt/CobaltTextToSpeechHelper.java',
            '../apk/app/src/main/java/foo/cobalt/media/CobaltPlaybackStateHelper.java',
            '../apk/app/src/main/java/foo/cobalt/media/MediaCodecBridge.java',
            '../apk/app/src/main/java/foo/cobalt/media/MediaCodecSelector.java',
            '../apk/app/src/main/java/foo/cobalt/media/MediaDrmBridge.java',
            '../apk/app/src/main/java/foo/cobalt/media/VideoSurfaceView.java',
            '../apk/app/src/main/java/foo/cobalt/PlatformError.java',
            '../apk/app/src/main/java/foo/cobalt/UsedByNative.java',
            '../apk/app/src/main/java/foo/cobalt/UserAuthorizer.java',
          ],
          'manifest': '../apk/app/src/main/AndroidManifest.xml',
          'keystore_path': 'debug.keystore',
          'resource_path': '../apk/app/src/main/res',
          # TODO we want this to be determined by the *_deploy target, but
          # we can't. Instead, we may have the deploy step generate shell
          # scripts to run the app launcher and fill this in there.
          # NOTE: this becomes "libstarboard.so" inside the APK.
          'lib': '<@(PRODUCT_DIR)/lib/libcobalt.so',
        },
        'inputs': [
          '<@(java_files)',
          '<@(manifest)',
          '>(lib)'
        ],
        'outputs': [
          'coat_debug.apk'
        ],
        'action': ['<(DEPTH)/starboard/android/tools/build_apk.py',
          '--verbose',
          '--debug',
          '--classpath', '<(javac_classpath)',
          '--build_tools_path', '<(android_build_tools_path)',
          '--min_sdk_version', '21',
          # TODO: Targeting Lollipop avoids runtime permissions for now
          # If you change the SDK versions, also change them in gradle.build
          '--target_sdk_version', '22',
          '--manifest', '<(manifest)',
          '--resources', '<(resource_path)',
          '--lib_arch', '<@(ANDROID_ABI)',
          '--lib', '>(lib)',
          '--keystore', '<(keystore_path)',
          '--ks_pass', 'pass:android',
          '--assets', '<@(PRODUCT_DIR)/content/data',
          '--outdir', '<@(PRODUCT_DIR)',
          '<@(java_files)'
        ]}]
    },
  ]
}
