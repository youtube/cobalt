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

// clang-format off
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
// clang-format on

#include "third_party/blink/renderer/platform/fonts/starboard/font_unique_name_lookup_starboard.h"

namespace blink {
sk_sp<SkTypeface> FontUniqueNameLookupStarboard::MatchUniqueName(
    const String& font_unique_name) {
  auto font_manager =
      font_manager_ ? font_manager_ : SkFontMgr_Cobalt::GetDefault();
  return font_manager->MatchFaceName(font_unique_name.Utf8().c_str());
}
}  // namespace blink
