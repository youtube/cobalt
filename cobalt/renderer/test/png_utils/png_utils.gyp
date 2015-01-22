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
  'targets': [
    {
      # This target provides (not production suitable) utility code for decoding
      # and encoding PNG files.  This is useful for sandboxes and tests for
      # quickly and easily loading in PNG files or saving PNG files as output.
      'target_name': 'png_utils',
      'type': 'static_library',
      'sources': [
        'png_decode.cc',
        'png_decode.h',
        'png_encode.cc',
        'png_encode.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
    },
  ],
}
