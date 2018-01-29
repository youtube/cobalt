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
    'glsl_shaders_dir': '<(DEPTH)/cobalt/renderer/glimp_shaders/glsl',
    'glsl_shaders': [
        '<!@(ls -1 <(DEPTH)/cobalt/renderer/glimp_shaders/glsl/*.glsl |xargs -n 1 basename)',
    ],
  }
}
