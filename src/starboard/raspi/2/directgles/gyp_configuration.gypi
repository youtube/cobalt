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
  'variables': {
    # Use the direct-to-GLES rasterizer.
    'rasterizer_type': 'direct-gles',

    # This controls the surface cache size for the fallback rasterizer (skia).
    # Since the direct-to-GLES rasterizer has its own cache, do not enable a
    # cache for the fallback rasterizer.
    'surface_cache_size_in_bytes': 0,

    # The rasterizer does not benefit much from rendering only the dirty
    # region. Disable this option since it costs GPU time.
    'render_dirty_region_only': 0,
  },

  'target_defaults': {
    'default_configuration': 'raspi-2-directgles_debug',
    'configurations': {
      'raspi-2-directgles_debug': {
        'inherit_from': ['debug_base'],
      },
      'raspi-2-directgles_devel': {
        'inherit_from': ['devel_base'],
      },
      'raspi-2-directgles_qa': {
        'inherit_from': ['qa_base'],
      },
      'raspi-2-directgles_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../architecture.gypi',
    '../../shared/gyp_configuration.gypi',
  ],
}
