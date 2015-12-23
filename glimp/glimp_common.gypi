# Copyright 2015 Google Inc. All Rights Reserved.
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
  'sources': [
    'base/move.h',
    'base/scoped_ptr.h',
    'egl/error.cc',
    'egl/error.h',
    'egl/display.cc',
    'egl/display.h',
    'egl/display_impl.h',
    'egl/display_registry.cc',
    'egl/display_registry.h',
    'egl/scoped_egl_lock.cc',
    'egl/scoped_egl_lock.h',
    'entry_points/egl.cc',
    'entry_points/egl_ext.cc',
    'entry_points/gles_2_0.cc',
    'entry_points/gles_2_0_ext.cc',
    'entry_points/gles_3_0.cc',
  ],

  'dependencies': [
    '<(DEPTH)/starboard/starboard.gyp:starboard',
  ],

  'include_dirs': [
    '<(DEPTH)/glimp/include',
  ],
}
