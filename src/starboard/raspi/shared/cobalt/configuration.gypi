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

# Cobalt-on-Linux-specific configuration.

{
  'variables': {
    'in_app_dial%': 0,

    # VideoCore's tiled renderer will do a render for every tile of a render
    # target even if only part of that target was rendered to.  Since the
    # scratch surface cache is designed to choose large offscreen surfaces so
    # that they can be maximally reused, it is not a very good fit for a tiled
    # renderer.
    'scratch_surface_cache_size_in_bytes' : 0,

    # This atlas size works better than the auto-mem setting.
    'skia_glyph_atlas_width%': '2048',
    'skia_glyph_atlas_height%': '2048',
  }, # end of variables
}
