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
  'targets': [
    {
      'target_name': 'blitter_backend',
      'type': 'static_library',

      # Sets up renderer::backend to use a Starboard Blitter API implementation.
      'sources': [
        'display.cc',
        'display.h',
        'graphics_context.cc',
        'graphics_context.h',
        'graphics_system.cc',
        'graphics_system.h',
        'surface_render_target.cc',
        'surface_render_target.h',
        'render_target.h',
      ],

      'dependencies': [
        '<(DEPTH)/starboard/starboard.gyp:starboard',
      ],
    },
  ],
}
