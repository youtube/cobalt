# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'configuration',
      'type': 'static_library',
      'sources': [
        'configuration.cc',
        'configuration.h',
      ],
      'conditions': [
        ['cobalt_user_on_exit_strategy != ""', {
          'defines': [
            'COBALT_USER_ON_EXIT_STRATEGY="<(cobalt_user_on_exit_strategy)"',
          ],
        }],
        ['render_dirty_region_only != -1', {
          'defines': [
            'COBALT_RENDER_DIRTY_REGION_ONLY=<(render_dirty_region_only)',
          ],
        }],
        ['cobalt_egl_swap_interval != -1', {
          'defines': [
            'COBALT_EGL_SWAP_INTERVAL=<(cobalt_egl_swap_interval)',
          ],
        }],
        ['fallback_splash_screen_url != ""', {
          'defines': [
            'COBALT_FALLBACK_SPLASH_SCREEN_URL="<(fallback_splash_screen_url)"',
          ],
        }],
        ['cobalt_enable_quic != -1', {
          'defines': [
            'COBALT_ENABLE_QUIC',
          ],
        }],
        ['skia_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_SKIA_CACHE_SIZE_IN_BYTES=<(skia_cache_size_in_bytes)',
          ],
        }],
        ['offscreen_target_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES=<(offscreen_target_cache_size_in_bytes)',
          ],
        }],
        ['encoded_image_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES=<(encoded_image_cache_size_in_bytes)',
          ],
        }],
        ['image_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_IMAGE_CACHE_SIZE_IN_BYTES=<(image_cache_size_in_bytes)',
          ],
        }],
        ['local_font_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES=<(local_font_cache_size_in_bytes)',
          ],
        }],
        ['remote_font_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES=<(remote_font_cache_size_in_bytes)',
          ],
        }],
        ['mesh_cache_size_in_bytes != -1', {
          'conditions': [
            ['mesh_cache_size_in_bytes == "auto"', {
              'defines': [
                'COBALT_MESH_CACHE_SIZE_IN_BYTES=1*1024*1024',
              ],
            }, {
              'defines': [
                'COBALT_MESH_CACHE_SIZE_IN_BYTES=<(mesh_cache_size_in_bytes)',
              ],
            }],
          ],
        }],
        ['software_surface_cache_size_in_bytes != -1', {
          'defines': [
            'COBALT_SOFTWARE_SURFACE_CACHE_SIZE_IN_BYTES=<(software_surface_cache_size_in_bytes)',
          ],
        }],
        ['image_cache_capacity_multiplier_when_playing_video != ""', {
          'defines': [
            'COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO=<(image_cache_capacity_multiplier_when_playing_video)'
          ],
        }],
        ['skia_glyph_atlas_width != -1', {
          'defines': [
            'COBALT_SKIA_GLYPH_ATLAS_WIDTH=<(skia_glyph_atlas_width)',
          ],
        }],
        ['skia_glyph_atlas_height != -1', {
          'defines': [
            'COBALT_SKIA_GLYPH_ATLAS_HEIGHT=<(skia_glyph_atlas_height)',
          ],
        }],
        ['reduce_cpu_memory_by != -1', {
          'defines': [
            'COBALT_REDUCE_CPU_MEMORY_BY=<(reduce_cpu_memory_by)',
          ],
        }],
        ['reduce_gpu_memory_by != -1', {
          'defines': [
            'COBALT_REDUCE_GPU_MEMORY_BY=<(reduce_gpu_memory_by)',
          ],
        }],
        ['cobalt_gc_zeal != -1', {
          'defines': [
            'COBALT_GC_ZEAL',
          ],
        }],
        ['rasterizer_type != ""', {
          'conditions': [
            ['rasterizer_type == "stub"', {
              'defines': [
                'COBALT_FORCE_STUB_RASTERIZER',
              ],
            }],
            ['rasterizer_type == "direct-gles"', {
              'defines': [
                'COBALT_FORCE_DIRECT_GLES_RASTERIZER',
              ],
            }],
          ],
        }],
        ['cobalt_enable_jit != -1', {
          'conditions': [
            ['cobalt_enable_jit == 1', {
              'defines': [ 'ENGINE_SUPPORTS_JIT' ],
            }, {
              'defines': [ 'COBALT_DISABLE_JIT' ],
            }],
          ],
        }],
        ['cobalt_enable_jit != -1', {
          'conditions': [
            ['cobalt_enable_jit == 1', {
              'defines': [ 'ENGINE_SUPPORTS_JIT' ],
            }, {
              'defines': [ 'COBALT_DISABLE_JIT' ],
            }],
          ],
        }],
      ],
    },
  ],
}
