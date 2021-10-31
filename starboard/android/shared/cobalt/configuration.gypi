# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

    'enable_account_manager': 1,

    # The 'android_system' font package installs only minimal fonts, with a
    # fonts.xml referencing the superset of font files we expect to find on any
    # Android platform. The Android SbFileOpen implementation falls back to
    # system fonts when it can't find the font file in the cobalt content.
    'cobalt_font_package': 'android_system',

    # Platform-specific implementations to compile into cobalt.
    'cobalt_platform_dependencies': [
      '<(DEPTH)/starboard/android/shared/cobalt/cobalt_platform.gyp:cobalt_platform',
    ],
  },
}
