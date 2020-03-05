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

# Translated from: skshaper.gni

{
  'variables': {
    'skshaper_src_path': '<(skia_modules_path)/skshaper/src',
    'skshaper_include_path': '<skia_modules_path)/skshaper/include',

    'skia_shaper_public': [
      "<(skshaper_include_path)/SkShaper.h"
    ],

    'skia_shaper_primitive_sources': [
      "<(skshaper_src_path)/SkShaper.cpp",
      "<(skshaper_src_path)/SkShaper_primitive.cpp",
    ],

    'skia_shaper_harfbuzz_sources': [
      "<(skshaper_src_path)/SkShaper_harfbuzz.cpp",
    ],
  },
}
