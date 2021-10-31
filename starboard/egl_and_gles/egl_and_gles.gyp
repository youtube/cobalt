# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

# The egl_and_gles.gyp:egl_and_gles gyp target can be depended upon in order to
# depend on a platform-specific implementation of EGL/GLES, whether that
# ultimately ends up being Angle on Windows, system libraries on Linux, or a
# custom implementation of EGL/GLES on an exotic platform. Depending on this
# target will set dependent settings to have the EGL and GLES system headers in
# their include directories.
#
# This decision is predicated on the value of the |gl_type| gyp variable defined
# in the gyp_configuration.gypi for the current platform.
{
  'targets': [
    {
      'variables': {
        'implementation_target': '<(DEPTH)/starboard/egl_and_gles/egl_and_gles_<(gl_type).gyp:egl_and_gles_implementation',
      },

      'target_name': 'egl_and_gles',
      'type': 'none',

      'dependencies': [
        '<(implementation_target)',
      ],
      'export_dependent_settings': [
        '<(implementation_target)',
      ],
    },
  ],
}
