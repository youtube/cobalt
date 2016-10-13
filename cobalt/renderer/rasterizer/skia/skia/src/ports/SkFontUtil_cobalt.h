/*
 * Copyright 2015 Google Inc.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTUTIL_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTUTIL_COBALT_H_

#include <bitset>
#include <map>
#include <utility>

#include "base/basictypes.h"
#include "SkString.h"
#include "SkTDArray.h"

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

typedef std::bitset<kNumCharactersPerPage> PageCharacters;
typedef std::map<int16, PageCharacters> CharacterMap;

bool IsCharacterValid(SkUnichar character);
int16 GetPage(SkUnichar character);
unsigned char GetPageCharacterIndex(SkUnichar character);

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

  FontFileInfo() : index(0), weight(0), style(kNormal_FontStyle) {}

  SkString file_name;
  int index;
  int weight;
  FontStyle style;

  SkString full_font_name;
  SkString postcript_name;
};

// A font family provides one or more names for a collection of fonts, each of
// which has a different style (normal, italic) or weight (thin, light, bold,
// etc).
// Cobalt distinguishes "fallback" fonts to support non-ASCII character sets.
// Page ranges are used to indicate the pages where a fallback font supports
// at least one character. This allows Cobalt to quickly determine fonts that
// cannot possible contain a character, without needing to load the font file
// and generate a full mapping of the font's characters.
struct FontFamily {
  FontFamily() : order(-1), is_fallback_font(false) {}

  SkTArray<SkString> names;
  SkTArray<FontFileInfo> fonts;
  SkLanguage language;
  int order;  // internal to SkFontConfigParser
  bool is_fallback_font;
  font_character_map::PageRanges page_ranges;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTUTIL_COBALT_H_
