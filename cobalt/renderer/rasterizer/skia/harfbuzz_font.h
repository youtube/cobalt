// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARFBUZZ_FONT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARFBUZZ_FONT_H_

#include <map>

#include "cobalt/renderer/rasterizer/skia/font.h"

#include "third_party/harfbuzz-ng/src/hb.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class HarfBuzzFontProvider {
 public:
  // Returns the HarfBuzz font that corresponds to the given Skia font.
  hb_font_t* GetHarfBuzzFont(Font* skia_font);
  void PurgeCaches();

 private:
  // Wrapper class for a HarfBuzz face created from a given Skia face.
  class HarfBuzzFace {
   public:
    HarfBuzzFace();
    ~HarfBuzzFace();

    void Init(const sk_sp<SkTypeface_Cobalt>& skia_face);

    hb_face_t* get();

   private:
    sk_sp<SkTypeface_Cobalt> typeface_;
    hb_face_t* face_;
  };

  std::map<SkFontID, HarfBuzzFace> face_cache_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARFBUZZ_FONT_H_
