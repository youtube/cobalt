# Copyright 2016 Google Inc. All Rights Reserved.
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

  'variables': {
    'variables': {
      'has_cdm%': '<!(test -e <(DEPTH)/third_party/cdm/cdm/include/content_decryption_module.h && echo 1 || echo 0)',
    },
    # This has_cdm gets exported to gyp files that include this one.
    'has_cdm%': '<(has_cdm)',
    'starboard_platform_sources': [
      '<(DEPTH)/starboard/linux/x64x11/main.cc',
      '<(DEPTH)/starboard/linux/x64x11/sanitizer_options.cc',
      '<(DEPTH)/starboard/linux/x64x11/system_get_property.cc',
      '<(DEPTH)/starboard/shared/starboard/link_receiver.cc',
      '<(DEPTH)/starboard/shared/x11/application_x11.cc',
      '<(DEPTH)/starboard/shared/x11/egl_swap_buffers.cc',
      '<(DEPTH)/starboard/shared/x11/window_create.cc',
      '<(DEPTH)/starboard/shared/x11/window_destroy.cc',
      '<(DEPTH)/starboard/shared/x11/window_get_platform_handle.cc',
      '<(DEPTH)/starboard/shared/x11/window_get_size.cc',
      '<(DEPTH)/starboard/shared/x11/window_internal.cc',
    ],
    'conditions': [
      ['has_cdm==1', {
        'starboard_platform_sources': [
          '<(DEPTH)/starboard/linux/x64x11/media_is_output_protected.cc',

          '<(DEPTH)/starboard/shared/starboard/drm/drm_close_session.cc',
          '<(DEPTH)/starboard/shared/starboard/drm/drm_destroy_system.cc',
          '<(DEPTH)/starboard/shared/starboard/drm/drm_generate_session_update_request.cc',
          '<(DEPTH)/starboard/shared/starboard/drm/drm_system_internal.h',
          '<(DEPTH)/starboard/shared/starboard/drm/drm_update_session.cc',

          '<(DEPTH)/starboard/shared/widevine/drm_create_system.cc',
          '<(DEPTH)/starboard/shared/widevine/drm_is_server_certificate_updatable.cc',
          '<(DEPTH)/starboard/shared/widevine/drm_system_widevine.cc',
          '<(DEPTH)/starboard/shared/widevine/drm_system_widevine.h',
          '<(DEPTH)/starboard/shared/widevine/drm_update_server_certificate.cc',
          '<(DEPTH)/starboard/shared/widevine/media_is_supported.cc',
        ],
      }],
    ],
  },
}
