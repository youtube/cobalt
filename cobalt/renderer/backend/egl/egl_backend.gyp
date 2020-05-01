# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
  'targets': [
    {
      'target_name': 'egl_backend',
      'type': 'static_library',

      'sources': [
        'display.cc',
        'display.h',
        'framebuffer.h',
        'framebuffer.cc',
        'framebuffer_render_target.h',
        'graphics_context.cc',
        'graphics_context.h',
        'graphics_system.cc',
        'graphics_system.h',
        'pbuffer_render_target.cc',
        'pbuffer_render_target.h',
        'render_target.h',
        'resource_context.cc',
        'resource_context.h',
        'texture.cc',
        'texture.h',
        'texture_data.cc',
        'texture_data.h',
        'texture_data_cpu.cc',
        'texture_data_cpu.h',
        'texture_data_pbo.cc',
        'texture_data_pbo.h',
        'utils.cc',
        'utils.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/configuration/configuration.gyp:configuration',
        '<(DEPTH)/starboard/starboard_headers_only.gyp:starboard_headers_only',
      ],
    },
  ],
}
