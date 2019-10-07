# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
  'includes': [
    # sets defaults for variables expected by cdm.gyp.
    '<(DEPTH)/third_party/ce_cdm/platforms/x86-64/settings.gypi',
    # cdm.gyp defines widevine_ce_cdm_static.
    '<(DEPTH)/third_party/ce_cdm/cdm/cdm.gyp',
  ],
  'variables': {
    'oemcrypto_target': 'oemcrypto',
    'optimize_target_for_speed': 1,

    # Use the protoc supplied in src/tools as precompiled executable.
    'protoc_dir': '<(DEPTH)/tools',
    'protoc_bin': '<(DEPTH)/tools/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'protoc': '<(DEPTH)/tools/protoc.exe',

    # Use the chromium target for protobuf.
    'protobuf_lib_type': 'target',
    'protobuf_lib': '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',

    'platform_oem_sources': [
      '<(DEPTH)/starboard/keyboxes/<(sb_widevine_platform)/<(sb_widevine_platform).h',
      '<(DEPTH)/starboard/keyboxes/<(sb_widevine_platform)/<(sb_widevine_platform)_client.c',
      '<(DEPTH)/starboard/shared/widevine/widevine_keybox_hash.cc',
      '<(DEPTH)/starboard/shared/widevine/wv_keybox.cc',
    ],
   },
  'target_defaults': {
    'include_dirs': [
      # Get protobuf headers from the chromium tree.
      '<(DEPTH)/third_party/protobuf/src',
    ],
    'dependencies': [
      '<(DEPTH)/third_party/boringssl/boringssl.gyp:crypto',
    ],
  },
  'targets': [
    {
      'target_name': 'oemcrypto',
      'type': 'static_library',
      'defines': [
        'COBALT_WIDEVINE_KEYBOX_TRANSFORM_FUNCTION=<(sb_widevine_platform)_client',
        'COBALT_WIDEVINE_KEYBOX_TRANSFORM_INCLUDE="starboard/keyboxes/<(sb_widevine_platform)/<(sb_widevine_platform).h"',
        'COBALT_WIDEVINE_KEYBOX_INCLUDE="<(DEPTH)/starboard/keyboxes/<(sb_widevine_platform)_widevine_keybox.h"',
      ],
      'sources': [
        '<@(platform_oem_sources)',
      ],
      'variables': {
        'oec_mock_dir': '<(DEPTH)/third_party/ce_cdm/oemcrypto/mock',
      },
      'dependencies': [
        '<(DEPTH)/third_party/ce_cdm/oemcrypto/mock/oec_mock.gyp:oec_mock',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/ce_cdm/core/include',
          '<(DEPTH)/third_party/ce_cdm/oemcrypto/include',
        ],
      },
    },
  ],
}
