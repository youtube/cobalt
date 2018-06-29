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
  'includes': ['<(DEPTH)/starboard/linux/shared/starboard_platform.gypi'],

  'variables': {
    'starboard_platform_sources': [
      '<(DEPTH)/starboard/contrib/linux/x64wl/main.cc',
      '<(DEPTH)/starboard/contrib/linux/x64wl/system_get_property.cc',
      '<(DEPTH)/starboard/contrib/linux/x64wl/player_output_mode_supported.cc',
      '<(DEPTH)/starboard/shared/starboard/link_receiver.cc',
      '<(DEPTH)/starboard/shared/wayland/application_wayland.cc',
      '<(DEPTH)/starboard/shared/wayland/dev_input.cc',
      '<(DEPTH)/starboard/shared/wayland/egl_workaround.cc',
      '<(DEPTH)/starboard/shared/wayland/native_display_type.cc',
      '<(DEPTH)/starboard/shared/wayland/window_create.cc',
      '<(DEPTH)/starboard/shared/wayland/window_destroy.cc',
      '<(DEPTH)/starboard/shared/wayland/window_get_platform_handle.cc',
      '<(DEPTH)/starboard/shared/wayland/window_get_size.cc',
      '<(DEPTH)/starboard/shared/wayland/window_internal.cc',
    ],
    'starboard_platform_sources!': [
      '<(DEPTH)/starboard/shared/linux/dev_input/dev_input.cc',
      '<(DEPTH)/starboard/shared/linux/dev_input/dev_input.h',
      '<(DEPTH)/starboard/shared/starboard/player/player_output_mode_supported.cc',
    ],
  },
  'target_defaults': {
    'include_dirs!': [
          '<(DEPTH)/third_party/khronos',
    ],
  },
}
