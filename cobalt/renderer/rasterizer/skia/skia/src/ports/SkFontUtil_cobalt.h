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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTUTIL_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTUTIL_COBALT_H_

#include <bitset>
#include <map>
#include <utility>

#include "SkMutex.h"
#include "SkString.h"
#include "SkTDArray.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

// The font_character_map namespace contains all of the constants, typedefs, and
// utility functions used with determining whether or not a font supports a
// character.
// The characters of a font are divided into pages, each of which encompasses
// 256 characters.
// Page ranges provide a quick and dirty method for determining whether or not a
// font potentially contains a character. These ranges indicate the pages where
// the font contains at least one character and are intended to minimize the
// number of fonts that must have their characters fully mapped. However, the
// inclusion of a page within a font's page ranges simply indicates that the
// font potentially contains the character. To determine whether or not the font
// does in fact contain the character requires loading the font's full character
// map. When a character map for a font is loaded, each page containing
// characters is added to a map and the page's contained characters are
// represented within a bitset of 256 bits.
namespace font_character_map {

static const int kMaxCharacterValue = 0x10ffff;
static const int kNumCharacterBitsPerPage = 8;
static const int kNumCharactersPerPage = 1 << kNumCharacterBitsPerPage;
static const int kMaxPageValue = kMaxCharacterValue >> kNumCharacterBitsPerPage;
static const int kPageCharacterIndexBitMask = kNumCharactersPerPage - 1;

// PageRange is a min-max pair of values that indicate that the font provides
// at least one character on all pages between the min and max value, inclusive.
typedef std::pair<int16, int16> PageRange;

// PageRanges is an array of PageRange objects that requires the objects to be
// in non-overlapping ascending order.
typedef SkTArray<PageRange> PageRanges;

bool IsCharacterValid(SkUnichar character);
int16 GetPage(SkUnichar character);
unsigned char GetPageCharacterIndex(SkUnichar character);

struct Character {
  SkGlyphID id = 0;
  bool is_set = false;
};

using PageCharacters = Character[kNumCharactersPerPage];
class CharacterMap : public base::RefCountedThreadSafe<CharacterMap> {
 public:
  const Character Find(SkUnichar character) {
    SkAutoMutexExclusive scoped_mutex(mutex_);

    int16 page = GetPage(character);
    std::map<int16, PageCharacters>::iterator page_iterator = data_.find(page);

    if (page_iterator == data_.end()) return {};

    unsigned char character_index = GetPageCharacterIndex(character);
    return page_iterator->second[character_index];
  }

  void Insert(SkUnichar character, SkGlyphID glyph) {
    SkAutoMutexExclusive scoped_mutex(mutex_);

    int16 page = GetPage(character);
    unsigned char character_index = GetPageCharacterIndex(character);
    data_[page][character_index] = {/* id= */ glyph, /* is_set= */ true};
  }

 private:
  std::map<int16, PageCharacters> data_;
  mutable SkMutex mutex_;
};

}  // namespace font_character_map

// The SkLanguage class represents a human written language, and is used by
// text draw operations to determine which glyph to draw when drawing
// characters with variants (ie Han-derived characters).
class SkLanguage {
 public:
  SkLanguage() {}
  explicit SkLanguage(const SkString& tag) : tag_(tag) {}
  explicit SkLanguage(const char* tag) : tag_(tag) {}
  SkLanguage(const char* tag, size_t len) : tag_(tag, len) {}
  SkLanguage(const SkLanguage& b) : tag_(b.tag_) {}

  // Gets a BCP 47 language identifier for this SkLanguage.
  // @return a BCP 47 language identifier representing this language
  const SkString& GetTag() const { return tag_; }

  // Performs BCP 47 fallback to return an SkLanguage one step more general.
  // @return an SkLanguage one step more general
  SkLanguage GetParent() const;

  bool operator==(const SkLanguage& b) const { return tag_ == b.tag_; }
  bool operator!=(const SkLanguage& b) const { return tag_ != b.tag_; }
  SkLanguage& operator=(const SkLanguage& b) {
    tag_ = b.tag_;
    return *this;
  }

 private:
  //! BCP 47 language identifier
  SkString tag_;
};

struct FontFileInfo {
  enum FontStyles {
    kNormal_FontStyle = 0x01,
    kItalic_FontStyle = 0x02,
  };
  typedef uint32_t FontStyle;

  FontFileInfo()
      : index(0),
        weight(400),
        style(kNormal_FontStyle),
        disable_synthetic_bolding(false) {}

  SkString file_name;
  int index;
  int weight;
  FontStyle style;
  bool disable_synthetic_bolding;

  SkString full_font_name;
  SkString postscript_name;
};

// A font family is a collection of fonts, which support the same characters,
// each of which has a different style (normal, italic) or weight (thin, light,
// bold, etc).
//
// Families can have any number of names, each of which can be used to look up
// the family.
//
// Additionally, families can be fallback families, which are used to support
// character sets that are unavailable within the named fonts.
//
// Page ranges are used with fallback families to indicate 256 character pages
// where it contains at least one character. This allows Cobalt to quickly
// determine that a family cannot support a character, without needing to
// generate a full mapping of the family's characters.
struct FontFamilyInfo {
  FontFamilyInfo()
      : is_fallback_family(true), fallback_priority(0), disable_caching(0) {}

  SkTArray<SkString> names;
  SkTArray<FontFileInfo> fonts;
  SkLanguage language;
  bool is_fallback_family;
  int fallback_priority;
  font_character_map::PageRanges page_ranges;
  bool disable_caching;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTUTIL_COBALT_H_
