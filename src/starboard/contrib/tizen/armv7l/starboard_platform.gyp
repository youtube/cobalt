# Copyright 2016 Samsung Electronics. All Rights Reserved.
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
  'includes': ['../shared/starboard_platform.gypi'],
  'targets': [
    {
      'target_name': 'starboard_platform',
      'type': 'static_library',
      'sources': [
        '<@(starboard_platform_sources)',
        '<(DEPTH)/starboard/contrib/tizen/armv7l/main.cc',
        '<(DEPTH)/starboard/contrib/tizen/armv7l/system_get_property.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_common.h',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_writer.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_dmp_writer.h',
      ],
      'defines': [
        'SB_PLAYER_ENABLE_VIDEO_DUMPER',
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<@(starboard_platform_dependencies)',
        '<(DEPTH)/starboard/contrib/tizen/armv7l/system.gyp:libpulse-simple',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:aul',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:capi-appfw-application',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:capi-network-connection',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:edbus',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:elementary',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:evas',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:gles20',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:tizen-extension-client',
        '<(DEPTH)/starboard/contrib/tizen/shared/system.gyp:wayland-egl',
      ],
    },
  ],
  'target_defaults': {
    'include_dirs!': [
          '<(DEPTH)/third_party/khronos',
    ],
  },
}
