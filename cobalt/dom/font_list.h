// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_FONT_LIST_H_
#define COBALT_DOM_FONT_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/glyph.h"
#include "cobalt/render_tree/glyph_buffer.h"

namespace cobalt {
namespace dom {

class FontCache;

// A specific font family within a font list. It has an internal state, which
// lets the font list know whether or not the font has already been requested,
// and if so, whether or not it was available. |font_| and |character_map_|
// will only be non-NULL in the case where |state_| is set to |kLoadedState|.
class FontListFont {
 public:
  enum State {
    kUnrequestedState,
    kLoadingWithTimerActiveState,
    kLoadingWithTimerExpiredState,
    kLoadedState,
    kUnavailableState,
    kDuplicateState,
  };

  explicit FontListFont(const std::string& family_name)
      : family_name_(family_name), state_(kUnrequestedState) {}

  const std::string& family_name() const { return family_name_; }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

  const scoped_refptr<render_tree::Font>& font() const { return font_; }
  void set_font(const scoped_refptr<render_tree::Font> font) { font_ = font; }

 private:
  std::string family_name_;
  State state_;

  // The render_tree::Font obtained via the font cache using |family_name_| in
  // font list font, along with |style_| and |size_| from the containing font
  // list. It is only non-NULL in the case where |state_| is set to
  // |kLoadedState|.
  scoped_refptr<render_tree::Font> font_;
};

// The key used for maps with a |FontList| value. It is also used for
// initializing the member variables of a |FontList| object.
struct FontListKey {
  FontListKey() : size(0) {}

  bool operator<(const FontListKey& rhs) const {
    if (size < rhs.size) {
      return true;
    } else if (rhs.size < size) {
      return false;
    } else if (style.weight < rhs.style.weight) {
      return true;
    } else if (rhs.style.weight < style.weight) {
      return false;
    } else if (style.slant < rhs.style.slant) {
      return true;
    } else if (rhs.style.slant < style.slant) {
      return false;
    } else {
      return family_names < rhs.family_names;
    }
  }

  std::vector<std::string> family_names;
  render_tree::FontStyle style;
  float size;
};

// |FontList| represents a unique font-style, font-weight, font-size, and
// font-family property combination within a document and is shared by all
// layout objects with matching font properties. It tracks a list of fonts,
// which it lazily requests from the font cache as required. It uses these to
// determine the font metrics for the font properties, as well as the specific
// fonts that should be used to render passed in text.
class FontList : public render_tree::FontProvider,
                 public base::RefCounted<FontList> {
 public:
  typedef base::SmallMap<
      std::map<render_tree::TypefaceId, scoped_refptr<render_tree::Font> >, 7>
      FallbackTypefaceToFontMap;
  typedef base::hash_map<int32, scoped_refptr<render_tree::Typeface> >
      CharacterFallbackTypefaceMap;
  typedef std::vector<FontListFont> FontListFonts;

  FontList(FontCache* font_cache, const FontListKey& font_list_key);

  bool IsVisible() const;

  // Reset loading fonts sets all font list fonts with a state of
  // |kLoadingState| back to |kUnrequestedState|, which will cause them to be
  // re-requested then next time they are needed.
  // If a font is encountered with a state of |kLoadingState| prior to the
  // first loaded font, then the primary font and its associated values are
  // reset, as they may change if the loading font is now available.
  void ResetLoadingFonts();

  // Given a string of text, returns the glyph buffer needed to render it. In
  // the case where |maybe_bounds| is non-NULL, it will also be populated with
  // the bounds of the rect.
  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, int32 text_length, bool is_rtl);

  // Given a string of text, return its width. This is faster than
  // CreateGlyphBuffer().
  float GetTextWidth(const char16* text_buffer, int32 text_length, bool is_rtl,
                     render_tree::FontVector* maybe_used_fonts);

  const render_tree::FontMetrics& GetFontMetrics();

  // Given a vector of fonts, provides the combined font metrics of all of the
  // fonts (including the primary font, regardless of whether it is present
  // in the vector).
  render_tree::FontMetrics GetFontMetrics(const render_tree::FontVector& fonts);

  // Returns the text run that signifies an ellipsis code point.
  char16 GetEllipsisValue() const;
  // Returns the first font in the font-list that supports the ellipsis code
  // point. In the case where the ellipsis font has not already been calculated,
  // it lazily generates it.
  const scoped_refptr<render_tree::Font>& GetEllipsisFont();
  // Returns the width of the ellipsis in the ellipsis font. In the case where
  // the width has not already been calculated, it lazily generates it.
  float GetEllipsisWidth();

  // Returns the width of the space in the first font in the font list that
  // supports the space character. In the case where the width has not already
  // been calculated, it lazily generates it.
  float GetSpaceWidth();

  // From render_tree::FontProvider

  const render_tree::FontStyle& style() const override { return style_; }
  float size() const override { return size_; }

  // Returns the first font in the font list that supports the specified
  // UTF-32 character or a fallback font provided by the font cache if none of
  // them do.
  // |GetPrimaryFont()| causes |RequestFont()| to be called on each font with a
  // state of |kUnrequestedState| in the list, until a font is encountered with
  // that has the specified character or all fonts in the list have been
  // requested.
  const scoped_refptr<render_tree::Font>& GetCharacterFont(
      int32 utf32_character, render_tree::GlyphIndex* glyph_index) override;

 private:
  const scoped_refptr<render_tree::Font>& GetFallbackCharacterFont(
      int32 utf32_character, render_tree::GlyphIndex* glyph_index);

  // The primary font is the first successfully loaded font among the font list
  // fonts. A loading font will potentially later become the primary font if it
  // successfully loads, but until then, a subsequent font will be used as the
  // primary one.
  // |GetPrimaryFont()| causes |RequestFont()| to be called on each font with a
  // state of |kUnrequestedState| in the list, until a font is encountered with
  // a state of |kLoadedState|.
  const scoped_refptr<render_tree::Font>& GetPrimaryFont();

  // Request a font from the font cache and update its state depending on the
  // results of the request. If the font is successfully set, then both its
  // |font_| and |character_map_| are non-NULL after this call.
  void RequestFont(size_t index);

  // Lazily generates the ellipsis font and ellipsis width. If it is already
  // generated then it immediately returns.
  void GenerateEllipsisInfo();

  // Lazily generates the space width. If it is already generated then it
  // immediately returns.
  void GenerateSpaceWidth();

  // The font cache, which provides both font family fonts and character
  // fallback fonts to the font list.
  FontCache* const font_cache_;

  FontListFonts fonts_;
  render_tree::FontStyle style_;
  float size_;

  // The first loaded font in the font list. Lazily generated the first time it
  // is requested. Used with font metrics and for generating the size of the
  // space character.
  scoped_refptr<render_tree::Font> primary_font_;

  // Font metrics are lazily generated the first time they are requested.
  bool is_font_metrics_set_;
  render_tree::FontMetrics font_metrics_;

  // Space width is lazily generated the first time it is requested.
  bool is_space_width_set_;
  float space_width_;

  // The ellipsis info is lazily generated the first time it is requested.
  bool is_ellipsis_info_set_;
  scoped_refptr<render_tree::Font> ellipsis_font_;
  float ellipsis_width_;

  // A mapping of the typeface to use with each fallback character. The font
  // list holds a reference to the map, which is owned by the font cache, and
  // shared between all font lists with a matching font style. If the font list
  // encounters a character that is not in the map, it populates the map with
  // the character itself, rather than relying on the cache to populate it.
  CharacterFallbackTypefaceMap& character_fallback_typeface_map_;

  // This is a mapping of a unique fallback typeface id to a specific fallback
  // font. By keeping this separate from the character fallback map, font lists
  // with different sizes but the same style can share the same character
  // fallback map.
  FallbackTypefaceToFontMap fallback_typeface_to_font_map_;

  // Allow the reference counting system access to our destructor.
  friend class base::RefCounted<FontList>;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_FONT_LIST_H_
