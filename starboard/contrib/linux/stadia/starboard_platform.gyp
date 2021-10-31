# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

# Note, that despite the file extension ".gyp", this file is included by several
# platform variants of linux-x64x11, like a ".gypi" file, since those platforms
# have no need to modify this code.
{
  'includes': ['<(DEPTH)/starboard/linux/x64x11/shared/starboard_platform_target.gypi'],

  'variables': {
    'starboard_platform_sources': [
      '<(DEPTH)/starboard/linux/stadia/main.cc',
      '<(DEPTH)/starboard/linux/x64x11/sanitizer_options.cc',
      '<(DEPTH)/starboard/linux/x64x11/system_get_property.cc',
      '<(DEPTH)/starboard/linux/x64x11/system_get_property_impl.cc',
      '<(DEPTH)/starboard/shared/libjpeg/image_decode.cc',
      '<(DEPTH)/starboard/shared/libjpeg/image_is_decode_supported.cc',
      '<(DEPTH)/starboard/shared/libjpeg/jpeg_image_decoder.cc',
      '<(DEPTH)/starboard/shared/libjpeg/jpeg_image_decoder.h',
      '<(DEPTH)/starboard/shared/starboard/link_receiver.cc',
      '<(DEPTH)/starboard/contrib/stadia/x11/application_stadia_x11.cc',
      '<(DEPTH)/starboard/shared/x11/egl_swap_buffers.cc',
      '<(DEPTH)/starboard/contrib/stadia/get_platform_service_api.cc',
      '<(DEPTH)/starboard/contrib/stadia/get_platform_service_api.h',
      '<(DEPTH)/starboard/contrib/stadia/stadia_interface.cc',
      '<(DEPTH)/starboard/contrib/stadia/stadia_interface.h',
      '<(DEPTH)/starboard/shared/x11/player_set_bounds.cc',
      '<(DEPTH)/starboard/shared/x11/window_create.cc',
      '<(DEPTH)/starboard/shared/x11/window_destroy.cc',
      '<(DEPTH)/starboard/shared/x11/window_get_platform_handle.cc',
      '<(DEPTH)/starboard/shared/x11/window_get_size.cc',
      '<(DEPTH)/starboard/shared/x11/window_internal.cc',
    ],
  },
}
