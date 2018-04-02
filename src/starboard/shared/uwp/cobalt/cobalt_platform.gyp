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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'cobalt_platform',
      'type': 'static_library',
      'sources': [
        'xb1_media_session_client.cc',
        'xb1_media_session_client.h',
        'xb1_media_session_updates.cc',
        'xhr_modify_headers.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/media_session/media_session.gyp:media_session',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalOptions': [
          '/ZW',           # Windows Runtime
          '/ZW:nostdlib',  # Windows Runtime, no default #using
          '/EHsc',         # C++ exceptions (required with /ZW)
          '/FU"<(visual_studio_install_path)/lib/x86/store/references/platform.winmd"',
          '/FU"<(windows_sdk_path)/References/<(windows_sdk_version)/Windows.Foundation.FoundationContract/3.0.0.0/Windows.Foundation.FoundationContract.winmd"',
          '/FU"<(windows_sdk_path)/References/<(windows_sdk_version)/Windows.Foundation.UniversalApiContract/4.0.0.0/Windows.Foundation.UniversalApiContract.winmd"',
          ],
        },
      },
      'defines': [
        # VS2017 always defines this for UWP apps
        'WINAPI_FAMILY=WINAPI_FAMILY_APP',
        # VS2017 always defines this for UWP apps
        '__WRL_NO_DEFAULT_LIB__',
      ],
    },
  ],
}
