/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARFBUZZ_FONT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARFBUZZ_FONT_H_

#include "cobalt/renderer/rasterizer/skia/font.h"

#include "third_party/harfbuzz-ng/src/hb.h"

class SkTypeface;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class SkiaFont;

hb_font_t* CreateHarfBuzzFont(SkiaFont* skia_font);

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARFBUZZ_FONT_H_
