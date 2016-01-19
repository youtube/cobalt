/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_FONT_CACHE_H_
#define COBALT_DOM_FONT_CACHE_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "cobalt/dom/font_face.h"
#include "cobalt/dom/font_list.h"
#include "cobalt/loader/font/remote_font_cache.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/resource_provider.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// The font cache is typically owned by dom::Document and handles the following:
//   - Creation and caching of font lists, which it provides to the used
//     style provider as requested. Font lists handle most layout-related font
//     cache interactions. Layout objects only interact with the font cache
//     through their font lists.
//   - Tracking of font faces, which it uses to determine if a specified
//     font family is local or remote, and for url determination in requesting
//     remote fonts.
//   - Retrieval of fonts, either locally from the resource provider or
//     remotely from the remote resource cache.
//   - Caching the indices of the glyphs that the typeface provides for specific
//     characters, so that only the first query of a specific font character
//     necessitates the glyph lookup.
//   - Determination of the fallback typeface for a specific character, and
//     caching of that information for subsequent lookups.
class FontCache {
 public:
  class RequestedRemoteFontInfo
      : public base::RefCounted<RequestedRemoteFontInfo> {
   public:
    RequestedRemoteFontInfo(
        const scoped_refptr<loader::font::CachedRemoteFont>& cached_remote_font,
        const base::Closure& font_load_event_callback);

    bool HasActiveRequestTimer() const { return request_timer_ != NULL; }
    void ClearRequestTimer() { request_timer_.reset(); }

   private:
    // The request timer delay, after which the requested font's fallback font
    // becomes visible.
    // NOTE: While using a timer of exactly 3 seconds is not specified by the
    // spec, it is the delay used by both Firefox and Webkit, and thus matches
    // user expectations.
    static const int kRequestTimerDelay = 3000;

    // The cached remote font reference both provides load event callbacks to
    // the remote font cache for this remote font, and also ensures that the
    // remote font is retained in the remote font cache's memory for as long as
    // this reference exists.
    scoped_ptr<loader::font::CachedRemoteFontReferenceWithCallbacks>
        cached_remote_font_reference_;

    // The request timer is started on object creation and triggers a load event
    // callback when the timer expires. Before the timer expires, font lists
    // that use this font will be rendered transparently to avoid a flash of
    // text from briefly displaying a fallback font. However, permanently hiding
    // the text while waiting for it to load is considered non-conformant
    // behavior by the spec, so after the timer expires, the fallback font
    // becomes visible (https://www.w3.org/TR/css3-fonts/#font-face-loading).
    scoped_ptr<base::Timer> request_timer_;
  };

  struct CharacterFallbackTypefaceKey {
    CharacterFallbackTypefaceKey(int32 key_character,
                                 const render_tree::FontStyle& key_style)
        : character(key_character), style(key_style) {}

    bool operator<(const CharacterFallbackTypefaceKey& rhs) const {
      if (character < rhs.character) {
        return true;
      } else if (rhs.character < character) {
        return false;
      } else if (style.weight < rhs.style.weight) {
        return true;
      } else if (rhs.style.weight < style.weight) {
        return false;
      } else {
        return style.slant < rhs.style.slant;
      }
    }

    int32 character;
    render_tree::FontStyle style;
  };

  // Font list related
  typedef std::map<FontListKey, scoped_refptr<FontList> > FontListMap;

  // Font-face related
  typedef std::map<std::string, FontFaceStyleSet> FontFaceMap;
  typedef std::map<GURL, scoped_refptr<RequestedRemoteFontInfo> >
      RequestedRemoteFontMap;

  // Character glyph map related
  typedef std::map<int32, render_tree::GlyphIndex> FontCharacterGlyphMap;
  typedef std::map<render_tree::TypefaceId, FontCharacterGlyphMap>
      TypefaceToFontCharacterGlyphMap;

  // Character fallback related
  typedef std::map<render_tree::TypefaceId, scoped_refptr<render_tree::Font> >
      FallbackTypefaceToFontMap;
  typedef std::map<CharacterFallbackTypefaceKey, render_tree::TypefaceId>
      CharacterFallbackTypefaceMap;

  FontCache(render_tree::ResourceProvider* resource_provider,
            loader::font::RemoteFontCache* remote_font_cache,
            const base::Closure& external_font_load_event_callback,
            const std::string& language);

  // Looks up the font list key in |font_list_map_|. If it doesn't exist, then
  // one is created. This is guaranteed to not return NULL.
  scoped_refptr<FontList> GetFontList(const FontListKey& font_list_key);

  // Removes any font lists that are only referenced by the font cache,
  // indicating that they are no longer being used.
  void RemoveUnusedFontLists();

  // Set a new font face map. If it matches the old font face map then nothing
  // is done. Otherwise, it is updated with the new value and the remote font
  // containers are purged of URLs that are no longer contained within the map.
  void SetFontFaceMap(scoped_ptr<FontFaceMap> font_face_map);

  // Retrieves the character glyph map associated with the font's typeface id.
  // If it does not exist, it is created. This is guaranteed to not return NULL.
  FontCharacterGlyphMap* GetFontCharacterGlyphMap(
      scoped_refptr<render_tree::Font> font);

  // Looks up the typeface id associated with a UTF-32 character and style. If
  // one is not associated yet, it requests the fallback font from the resource
  // provider and maps the character to the typeface. Additionally, if the
  // typeface id is not currently mapped to a font, this mapping is made as
  // well, allowing the typeface to be cloned in various sizes.
  // After this call, the returned typeface id is guaranteed to return a
  // non-NULL font when passed to |GetFallbackFont()|.
  render_tree::TypefaceId GetCharacterFallbackTypefaceId(
      int32 utf32_character, render_tree::FontStyle style, float size);

  // Looks up a fallback font associated with a typeface id and creates a clone
  // with the requested size. If there is no fallback font associated with the
  // typeface id, then it returns NULL.
  scoped_refptr<render_tree::Font> GetFallbackFont(
      render_tree::TypefaceId typeface_id, float size);

  // Attempts to retrieve a font. If the family maps to a font face, then this
  // makes a request to |TryGetRemoteFont()|; otherwise, it makes a request
  // to |TryGetLocalFont()|. This function may return NULL.
  scoped_refptr<render_tree::Font> TryGetFont(const std::string& family,
                                              render_tree::FontStyle style,
                                              float size,
                                              FontListFont::State* state);

 private:
  // Returns the font if it is in the remote font cache and available;
  // otherwise returns NULL.
  // If the font is in the cache, but is not loaded, this call triggers an
  // asynchronous load of it. Both an external load even callback, provided by
  // the constructor and an |OnRemoteFontLoadEvent| callback provided by the
  // font are registered with the remote font cache to be called when the load
  // finishes.
  // If the font is loading but not currently available, |maybe_is_font_loading|
  // will be set to true.
  scoped_refptr<render_tree::Font> TryGetRemoteFont(const GURL& url, float size,
                                                    FontListFont::State* state);

  // Returns NULL if the requested family is not empty and is not available in
  // the resource provider. Otherwise, returns the best matching local font.
  // |maybe_is_font_loading| is always set to false.
  scoped_refptr<render_tree::Font> TryGetLocalFont(const std::string& family,
                                                   render_tree::FontStyle style,
                                                   float size,
                                                   FontListFont::State* state);

  // Called when a remote font either successfully loads or fails to load. In
  // either case, the event can impact the fonts contained within the font
  // lists. As a result, the font lists need to have their loading fonts reset
  // so that they'll be re-requested from the cache.
  void OnRemoteFontLoadEvent(const GURL& url);

  render_tree::ResourceProvider* const resource_provider_;
  // TODO(***REMOVED***): Explore eliminating the remote font cache and moving its
  // logic into the font cache when the loader interface improves.
  loader::font::RemoteFontCache* const remote_font_cache_;
  const base::Closure external_font_load_event_callback_;
  const std::string language_;

  // Font list related
  // This maps unique font property combinations that are currently in use
  // within the document to the font lists that provides the functionality
  // associated with those font property combinations.
  FontListMap font_list_map;

  // Font-face related
  // The cache contains a map of font faces and handles requesting fonts by url
  // on demand from |remote_font_cache_| with a load event callback provided by
  // the constructor. Cached remote fonts returned by |remote_font_cache_| have
  // a reference retained by the cache for as long as the cache contains a font
  // face with the corresponding url, to ensure that they remain in memory.
  scoped_ptr<FontFaceMap> font_face_map_;
  RequestedRemoteFontMap requested_remote_font_cache_;

  // Character glyph map related
  // This map tracks the glyphs a typeface provides for specific characters.
  // Each unique typeface id maps to a character glyph map (allowing all fonts
  // sharing the same typeface id to share the same character glyph map).
  // Character to glyph entries are lazily populated the first time that they
  // are requested. This ensures that the resource provider will only be queried
  // for typeface-character combinations that are actually used, and will never
  // be queried more than once for any given typeface-character combination.
  TypefaceToFontCharacterGlyphMap typeface_id_to_character_glyph_map_;

  // Fallback font related
  // Contains maps of both the unique typeface to use with a specific
  // character-style combination and a prototype font for the typeface, which
  // can be used to provide copies of the font at any desired size, without
  // requiring an additional request of the resource provider for each newly
  // encountered size.
  CharacterFallbackTypefaceMap character_fallback_typeface_map_;
  FallbackTypefaceToFontMap fallback_typeface_to_font_map_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_FONT_CACHE_H_
