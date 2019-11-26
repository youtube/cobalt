# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
    'blitter_stub_sources': [
      '<(DEPTH)/starboard/shared/stub/blitter_blit_rects_to_rects.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_blit_rect_to_rect.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_blit_rect_to_rect_tiled.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_create_context.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_create_default_device.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_create_pixel_data.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_create_render_target_surface.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_create_surface_from_pixel_data.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_create_swap_chain_from_window.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_destroy_context.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_destroy_device.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_destroy_pixel_data.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_destroy_surface.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_destroy_swap_chain.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_download_surface_pixels.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_fill_rect.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_flip_swap_chain.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_flush_context.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_get_max_contexts.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_get_pixel_data_pitch_in_bytes.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_get_pixel_data_pointer.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_get_render_target_from_surface.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_get_render_target_from_swap_chain.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_get_surface_info.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_is_blitter_supported.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_is_pixel_format_supported_by_download_surface_pixels.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_is_pixel_format_supported_by_pixel_data.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_is_surface_format_supported_by_render_target_surface.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_set_blending.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_set_color.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_set_modulate_blits_with_color.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_set_render_target.cc',
      '<(DEPTH)/starboard/shared/stub/blitter_set_scissor.cc',
    ]
  }
}