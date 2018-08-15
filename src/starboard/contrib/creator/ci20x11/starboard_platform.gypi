# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
    'starboard_platform_sources': [
      '<(DEPTH)/starboard/contrib/creator/ci20x11/atomic_public.h',
      '<(DEPTH)/starboard/contrib/creator/ci20x11/configuration_public.h',
      '<(DEPTH)/starboard/contrib/creator/ci20x11/main.cc',
      '<(DEPTH)/starboard/contrib/creator/ci20x11/egl_workaround.cc',
      '<(DEPTH)/starboard/contrib/creator/ci20x11/system_get_property.cc',
      '<(DEPTH)/starboard/shared/starboard/link_receiver.cc',
      '<(DEPTH)/starboard/shared/x11/application_x11.cc',
      '<(DEPTH)/starboard/shared/x11/egl_swap_buffers.cc',
      '<(DEPTH)/starboard/shared/x11/player_set_bounds.cc',
      '<(DEPTH)/starboard/shared/x11/window_create.cc',
      '<(DEPTH)/starboard/shared/x11/window_destroy.cc',
      '<(DEPTH)/starboard/shared/x11/window_get_platform_handle.cc',
      '<(DEPTH)/starboard/shared/x11/window_get_size.cc',
      '<(DEPTH)/starboard/shared/x11/window_internal.cc',
    ],

    # Exclude shared implementations specified by the included .gypi if this
    # file already specifies a platform-specific version.
    'starboard_platform_sources!': [
      '<(DEPTH)/starboard/shared/starboard/player/player_set_bounds.cc',
    ],
  },
}
