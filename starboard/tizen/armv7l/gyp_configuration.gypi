# Copyright 2016 Samsung Electronics. All Rights Reserved.
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
    'target_arch': 'arm',

    'gl_type': 'system_gles2',

    # Tizen uses ARMv7
    'arm_version': 7,
    'armv7': 1,
    'arm_neon': 0,

    # Reduce garbage collection threshold from the default of 8MB in order to
    # save on memory.  This will mean that garbage collection occurs more
    # frequently.
    'mozjs_garbage_collection_threshold_in_bytes%': 4 * 1024 * 1024,

    # The rasterizer does not benefit much from rendering only the dirty
    # region. Disable this option since it costs GPU time.
    'render_dirty_region_only': 0,

    'cobalt_media_buffer_storage_type': 'memory',
    'cobalt_media_buffer_initial_capacity': 26 * 1024 * 1024,
    'cobalt_media_buffer_allocation_unit': 0 * 1024 * 1024,
    'cobalt_media_buffer_non_video_budget': 5 * 1024 * 1024,
    'cobalt_media_buffer_video_budget_1080p': 16 * 1024 * 1024,
    'cobalt_media_buffer_video_budget_4k': 60 * 1024 * 1024,

    'platform_libraries': [
      '-lasound',
      '-lavcodec',
      '-lavformat',
      '-lavutil',
      '-ldlog',
    ],
  },

  'target_defaults': {
    'default_configuration': 'tizen-armv7l_debug',
    'configurations': {
      'tizen-armv7l_debug': {
        'inherit_from': ['debug_base'],
      },
      'tizen-armv7l_devel': {
        'inherit_from': ['devel_base'],
      },
      'tizen-armv7l_qa': {
        'inherit_from': ['qa_base'],
      },
      'tizen-armv7l_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  }, # end of target_defaults

  'includes': [
    '../shared/gyp_configuration.gypi',
  ],
}
