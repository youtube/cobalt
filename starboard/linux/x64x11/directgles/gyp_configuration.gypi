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
  'variables': {
    # Use the direct-to-GLES rasterizer.
    # This rasterizer falls back to the hardware skia rasterizer in certain
    # situations.
    # NOTE: This rasterizer allocates offscreen render targets upfront,
    # including a full screen scratch surface.
    'rasterizer_type': 'direct-gles',

    # Accommodate the direct-to-GLES rasterizer's additional memory overhead
    # by reducing the image cache size. This is only an issue when additional
    # memory is required for video frames, so shrink the image cache size
    # only during video playback.
    'image_cache_capacity_multiplier_when_playing_video': '0.5f',

    # This dictates how much GPU memory will be used for the offscreen render
    # target atlases. One will be used as a cache and the other used as a
    # working scratch. It is recommended to allot enough memory for two
    # atlases that are roughly a quarter of the framebuffer. (The render
    # target atlases use power-of-2 dimenions.) If the target web app frequently
    # uses effects which require offscreen targets, then more memory should be
    # reserved for optimal performance.
    'surface_cache_size_in_bytes': 2 * (1024 * 512 * 4),

    # The rasterizer does not benefit much from rendering only the dirty
    # region. Disable this option since it costs GPU time.
    'render_dirty_region_only': 0,
  },
  'target_defaults': {
    'default_configuration': 'linux-x64x11-directgles_debug',
    'configurations': {
      'linux-x64x11-directgles_debug': {
        'inherit_from': ['debug_base'],
      },
      'linux-x64x11-directgles_devel': {
        'inherit_from': ['devel_base'],
      },
      'linux-x64x11-directgles_qa': {
        'inherit_from': ['qa_base'],
      },
      'linux-x64x11-directgles_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../gyp_configuration.gypi',
  ],
}
