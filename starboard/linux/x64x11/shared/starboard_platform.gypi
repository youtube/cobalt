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
  'includes': ['../../shared/starboard_platform.gypi'],

  'variables': {
    'starboard_platform_sources': [
      '<(DEPTH)/starboard/linux/x64x11/main.cc',
      '<(DEPTH)/starboard/linux/x64x11/sanitizer_options.cc',
      '<(DEPTH)/starboard/linux/x64x11/system_get_property.cc',
      '<(DEPTH)/starboard/linux/x64x11/system_get_property_impl.cc',
      '<(DEPTH)/starboard/shared/libjpeg/image_decode.cc',
      '<(DEPTH)/starboard/shared/libjpeg/image_is_decode_supported.cc',
      '<(DEPTH)/starboard/shared/libjpeg/jpeg_image_decoder.cc',
      '<(DEPTH)/starboard/shared/libjpeg/jpeg_image_decoder.h',
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

    'starboard_platform_dependencies': [
      '<(DEPTH)/starboard/egl_and_gles/egl_and_gles.gyp:egl_and_gles',
      '<(DEPTH)/third_party/libjpeg-turbo/libjpeg.gyp:libjpeg',
    ],

    # Exclude shared implementations specified by the included .gypi if this
    # file already specifies a platform-specific version.
    'starboard_platform_sources!': [
      '<(DEPTH)/starboard/shared/starboard/player/player_set_bounds.cc',
      '<(DEPTH)/starboard/shared/stub/image_decode.cc',
      '<(DEPTH)/starboard/shared/stub/image_is_decode_supported.cc',
    ],

    'variables': {
      'has_private_system_properties%': '<!(test -e <(DEPTH)/starboard/keyboxes/linux/system_properties.cc && echo 1 || echo 0)',
    },
    # This has_private_system_properties gets exported to gyp files that include this one.
    'has_private_system_properties%': '<(has_private_system_properties)',
    'conditions': [
      ['has_private_system_properties==1', {
        'starboard_platform_sources': [
          '<(DEPTH)/starboard/keyboxes/linux/system_properties.cc',
        ],
      }, {
        'starboard_platform_sources': [
          '<(DEPTH)/starboard/linux/x64x11/public_system_properties.cc',
        ],
      }],
    ],
  },
}
