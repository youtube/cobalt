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

# This file defines 'glsl_keys', which is an input to glimp's
# map_glsl_shaders.gypi tool used to map GLSL shaders to platform-specific
# shaders, mapping them by filename.  The 'glsl_keys' variable defined here
# lists all GLSL shaders intended to be used by Cobalt.  Make sure that this
# is included before including 'glimp/map_glsl_shaders.gypi'.

{
  'variables': {
    'glsl_shaders': [
      # A simple shader allowing for full-screen quad blitting, used to enable
      # the transfer of a software-rasterized image to the display.
      '<(DEPTH)/cobalt/renderer/glimp_shaders/glsl/fragment_position_and_texcoord.glsl',
      '<(DEPTH)/cobalt/renderer/glimp_shaders/glsl/vertex_position_and_texcoord.glsl',
    ],
  }
}
