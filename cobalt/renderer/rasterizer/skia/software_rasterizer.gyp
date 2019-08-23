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
  'variables': {
    'optimize_target_for_speed': 1,
  },
  'targets': [
    {
      'target_name': 'software_rasterizer',
      'type': 'static_library',

      'sources': [
        'software_image.cc',
        'software_image.h',
        'software_mesh.h',
        'software_rasterizer.cc',
        'software_rasterizer.h',
        'software_resource_provider.cc',
        'software_resource_provider.h',
      ],

      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/common.gyp:common',
        '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/ots/ots.gyp:ots',
      ],
    },
  ],
}
