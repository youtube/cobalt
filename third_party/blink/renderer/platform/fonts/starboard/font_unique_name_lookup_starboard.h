// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_STARBOARD_FONT_UNIQUE_NAME_LOOKUP_STARBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_STARBOARD_FONT_UNIQUE_NAME_LOOKUP_STARBOARD_H_

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"

namespace blink {

class FontUniqueNameLookupStarboard : public FontUniqueNameLookup {
 public:
  FontUniqueNameLookupStarboard() = default;
  FontUniqueNameLookupStarboard(const FontUniqueNameLookupStarboard&) = delete;
  FontUniqueNameLookupStarboard& operator=(
      const FontUniqueNameLookupStarboard&) = delete;
  ~FontUniqueNameLookupStarboard() = default;

  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override;

 private:
  sk_sp<SkFontMgr_Cobalt> font_manager_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_STARBOARD_FONT_UNIQUE_NAME_LOOKUP_STARBOARD_H_
