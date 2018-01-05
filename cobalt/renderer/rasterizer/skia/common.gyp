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

{
  'targets': [
    {
      'target_name': 'common',
      'type': 'static_library',

      'sources': [
        'cobalt_skia_type_conversions.cc',
        'cobalt_skia_type_conversions.h',
        'font.cc',
        'font.h',
        'glyph_buffer.cc',
        'glyph_buffer.h',
        'harfbuzz_font.cc',
        'harfbuzz_font.h',
        'image.cc',
        'image.h',
        'render_tree_node_visitor.cc',
        'render_tree_node_visitor.h',
        'scratch_surface_cache.cc',
        'scratch_surface_cache.h',
        'text_shaper.cc',
        'text_shaper.h',
        'typeface.cc',
        'typeface.h',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/ots/ots.gyp:ots',
      ],
    },
  ],
}
