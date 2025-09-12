// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"

namespace font_character_map {

bool IsCharacterValid(SkUnichar character) {
  return character >= 0 && character <= kMaxCharacterValue;
}

int16 GetPage(SkUnichar character) {
  return static_cast<int16>(character >> kNumCharacterBitsPerPage);
}

unsigned char GetPageCharacterIndex(SkUnichar character) {
  return static_cast<unsigned char>(character & kPageCharacterIndexBitMask);
}

}  // namespace font_character_map

SkLanguage SkLanguage::GetParent() const {
  SkASSERT(!tag_.isEmpty());
  const char* tag = tag_.c_str();

  // strip off the rightmost "-.*"
  const char* parent_tag_end = strrchr(tag, '-');
  if (parent_tag_end == NULL) {
    return SkLanguage();
  }
  size_t parent_tag_len = parent_tag_end - tag;
  return SkLanguage(tag, parent_tag_len);
}
