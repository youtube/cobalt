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

# Cobalt-on-Android-specific configuration for Starboard.

{
  'variables': {
    'in_app_dial': 0,

    'custom_media_session_client': 1,
    'enable_account_manager': 1,
    'enable_map_to_mesh': 1,

    # Some Android devices do not advertise their support for
    # EGL_SWAP_BEHAVIOR_PRESERVED_BIT properly, so, we play it safe and disable
    # relying on it for Android.
    'render_dirty_region_only': 0,

    # The 'android_system' font package installs only minimal fonts, with a
    # fonts.xml referencing the superset of font files we expect to find on any
    # Android platform. The Android SbFileOpen implementation falls back to
    # system fonts when it can't find the font file in the cobalt content.
    'cobalt_font_package': 'android_system',

    'cobalt_media_buffer_initial_capacity': 80 * 1024 * 1024,
    'cobalt_media_buffer_max_capacity_1080p': 0,
    'cobalt_media_buffer_max_capacity_4k': 0,


    # On Android, we almost never want to actually terminate the process, so
    # instead indicate that we would instead like to be suspended when users
    # initiate an "exit".
    'cobalt_user_on_exit_strategy': 'suspend',

    # Switch Android's SurfaceFlinger queue to "async mode" so that we don't
    # queue up rendered frames which would interfere with frame timing and
    # more importantly lead to input latency.
    'cobalt_egl_swap_interval': 0,

    # Platform-specific implementations to compile into cobalt.
    'cobalt_platform_dependencies': [
      '<(DEPTH)/starboard/android/shared/cobalt/cobalt_platform.gyp:cobalt_platform',
    ],

    # Platform-specific interfaces to inject into Cobalt's JavaScript 'window'
    # global object.
    'cobalt_webapi_extension_source_idl_files': [
      'android.idl',
      'feedback_service.idl',
    ],

    # Platform-specific IDL interface implementations.
    'cobalt_webapi_extension_gyp_target':
    '<(DEPTH)/starboard/android/shared/cobalt/android_webapi_extension.gyp:android_webapi_extension',
  },

  'target_defaults': {
    'target_conditions': [
      ['<!(python -c "import os.path; print os.path.isfile(\'<(DEPTH)/starboard/android/shared/private/keys.h\') & 1 | 0")==1', {
        'defines': ['SB_HAS_SPEECH_API_KEY'],
      }],
    ],
  }, # end of target_defaults
}
