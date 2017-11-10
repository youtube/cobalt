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

{
  # Angle configuration for translating EGL/GLES -> GLX/OpenGL.
  'variables': {
    # Sets the default value of 'gl_type' to angle, so that other Linux
    # configurations could override it.
    'gl_type%': 'angle',
    'angle_build_winrt': 0,
    'angle_enable_gl': 1,
    'angle_enable_d3d11': 0,
    'angle_link_glx': 0,
    'angle_use_glx': 1,
    'angle_platform_linux': 1,
    'angle_platform_posix': 1,
  },
}
