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
  'defines': [
    'COBALT_SKIA_CACHE_SIZE_IN_BYTES=<(skia_cache_size_in_bytes)',
    'COBALT_SCRATCH_SURFACE_CACHE_SIZE_IN_BYTES=<(scratch_surface_cache_size_in_bytes)',
    'COBALT_SURFACE_CACHE_SIZE_IN_BYTES=<(surface_cache_size_in_bytes)',
  ],
  'conditions': [
    ['rasterizer_type == "software"', {
      'defines': [
        'COBALT_FORCE_SOFTWARE_RASTERIZER',
      ],
    }],
    ['rasterizer_type == "stub"', {
      'defines': [
        'COBALT_FORCE_STUB_RASTERIZER',
      ],
    }],
  ],
}
