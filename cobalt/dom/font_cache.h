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

#ifndef COBALT_DOM_FONT_CACHE_H_
#define COBALT_DOM_FONT_CACHE_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/dom/font_face.h"
#include "cobalt/dom/font_list.h"
#include "cobalt/dom/location.h"
#include "cobalt/loader/font/remote_typeface_cache.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph.h"
#include "cobalt/render_tree/resource_provider.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// The font cache is typically owned by dom::Document and handles the following:
//   - Tracking of font faces, which it uses to determine if a specified
//     font family is local or remote, and for url determination in requesting
//     remote typefaces.
//   - Creation and caching of font lists, which it provides to the used
//     style provider as requested. Font lists handle most layout-related font
//     cache interactions. Layout objects only interact with the font cache
//     through their font lists.
//   - Retrieval of typefaces, either locally from the resource provider or
//     remotely from the remote typeface cache, and caching of both typefaces
//     and fonts to facilitate sharing of them across font lists.
//   - Determination of the fallback typeface for a specific character using a
//     specific font style, and caching of that information for subsequent
//     lookups.
//   - Creation of glyph buffers, which is accomplished by passing the request
//    to the resource provider.
// NOTE: The font cache is not thread-safe and must only used within a single
// thread.
class FontCache {
 public:
  class RequestedRemoteTypefaceInfo
      : public base::RefCounted<RequestedRemoteTypefaceInfo> {
   public:
    RequestedRemoteTypefaceInfo(
        const scoped_refptr<loader::font::CachedRemoteTypeface>&
            cached_remote_typeface,
        const base::Closure& font_load_event_callback);

    bool HasActiveRequestTimer() const { return request_timer_ != NULL; }
    void ClearRequestTimer() { request_timer_.reset(); }

   private:
    // The request timer delay, after which the requesting font list's fallback
    // font becomes visible.
    // NOTE: While using a timer of exactly 3 seconds is not specified by the
    // spec, it is the delay used by both Firefox and Webkit, and thus matches
    // user expectations.
    static const int kRequestTimerDelay = 3000;

    // The cached remote typeface reference both provides load event callbacks
    // to the remote typeface cache for this remote typeface, and also ensures
    // that the remote typeface is retained in the remote typeface cache's
    // memory for as long as this reference exists.
    scoped_ptr<loader::font::CachedRemoteTypefaceReferenceWithCallbacks>
        cached_remote_typeface_reference_;

    // The request timer is started on object creation and triggers a load event
    // callback when the timer expires. Before the timer expires, font lists
    // that use this font will be rendered transparently to avoid a flash of
    // text from briefly displaying a fallback font. However, permanently hiding
    // the text while waiting for it to load is considered non-conformant
    // behavior by the spec, so after the timer expires, the fallback font
    // becomes visible (https://www.w3.org/TR/css3-fonts/#font-face-loading).
    scoped_ptr<base::Timer> request_timer_;
  };

  struct FontListInfo {
    scoped_refptr<FontList> font_list;
    base::TimeTicks inactive_time;
  };

  struct FontKey {
    FontKey(render_tree::TypefaceId key_typeface_id, float key_size)
        : typeface_id(key_typeface_id), size(key_size) {}

    bool operator<(const FontKey& rhs) const {
      if (typeface_id != rhs.typeface_id) {
        return typeface_id < rhs.typeface_id;
      } else {
        return size < rhs.size;
      }
    }

    render_tree::TypefaceId typeface_id;
    float size;
  };

  struct FontInfo {
    scoped_refptr<render_tree::Font> font;
    base::TimeTicks inactive_time;
  };

  struct InactiveFontKey {
    InactiveFontKey(base::TimeTicks time, FontKey key)
        : inactive_time(time), font_key(key) {}

    bool operator<(const InactiveFontKey& rhs) const {
      if (inactive_time != rhs.inactive_time) {
        return inactive_time < rhs.inactive_time;
      } else {
        return font_key < rhs.font_key;
      }
    }

    base::TimeTicks inactive_time;
    FontKey font_key;
  };

  struct CharacterFallbackKey {
    explicit CharacterFallbackKey(const render_tree::FontStyle& key_style)
        : style(key_style) {}

    bool operator<(const CharacterFallbackKey& rhs) const {
      if (style.weight != rhs.style.weight) {
        return style.weight < rhs.style.weight;
      } else {
        return style.slant < rhs.style.slant;
      }
    }

    render_tree::FontStyle style;
  };

  // Font-face related
  typedef std::map<std::string, FontFaceStyleSet> FontFaceMap;
  typedef std::map<GURL, scoped_refptr<RequestedRemoteTypefaceInfo> >
      RequestedRemoteTypefaceMap;

  // Font list related
  typedef std::map<FontListKey, FontListInfo> FontListMap;

  // Typeface/Font related
  typedef base::SmallMap<
      std::map<render_tree::TypefaceId, scoped_refptr<render_tree::Typeface> >,
      7> TypefaceMap;
  typedef std::map<FontKey, FontInfo> FontMap;
  typedef std::set<InactiveFontKey> InactiveFontSet;

  // Character fallback related
  typedef base::hash_map<int32, scoped_refptr<render_tree::Typeface> >
      CharacterFallbackTypefaceMap;
  typedef std::map<CharacterFallbackKey, CharacterFallbackTypefaceMap>
      CharacterFallbackTypefaceMaps;

  FontCache(render_tree::ResourceProvider** resource_provider,
            loader::font::RemoteTypefaceCache* remote_typeface_cache,
            const base::Closure& external_typeface_load_event_callback,
            const std::string& language_script,
            scoped_refptr<Location> document_location);

  // Set a new font face map. If it matches the old font face map then nothing
  // is done. Otherwise, it is updated with the new value and the remote
  // typeface containers are purged of URLs that are no longer contained within
  // the map.
  void SetFontFaceMap(scoped_ptr<FontFaceMap> font_face_map);

  // Purge all caches within the font cache.
  void PurgeCachedResources();

  // Process unused font lists and fonts, potentially purging them from the
  // cache if they meet the removal requirements.
  void ProcessInactiveFontListsAndFonts();

  // Looks up and returns the font list in |font_list_map_|. If the font list
  // doesn't already exist, then a new one is created and added to the cache.
  const scoped_refptr<FontList>& GetFontList(const FontListKey& font_list_key);

  // Looks up and returns the font with the matching typeface and size in
  // |font_map_|. If it doesn't already exist in the cache, then a new font is
  // created from typeface and added to the cache.
  const scoped_refptr<render_tree::Font>& GetFontFromTypefaceAndSize(
      const scoped_refptr<render_tree::Typeface>& typeface, float size);

  // Attempts to retrieve a font. If the family maps to a font face, then this
  // makes a request to |TryGetRemoteFont()|; otherwise, it makes a request
  // to |TryGetLocalFont()|. This function may return NULL.
  scoped_refptr<render_tree::Font> TryGetFont(const std::string& family,
                                              render_tree::FontStyle style,
                                              float size,
                                              FontListFont::State* state);

  // Returns the character fallback typeface map associated with the specified
  // style. Each unique style has its own exclusive map. If it doesn't already
  // exist in the cache, then it is created during the request.
  // NOTE: This map is provided by the font cache so that all font lists with
  // the same style can share the same map. However, the cache itself does not
  // populate or query the map.
  CharacterFallbackTypefaceMap& GetCharacterFallbackTypefaceMap(
      const render_tree::FontStyle& style);

  // Retrieves the typeface associated with a UTF-32 character and style from
  // the resource provider.
  // NOTE: |character_fallback_typeface_maps_| is not queried before retrieving
  // the typeface from the resource provider. It is expected that the font list
  // will query its specific map first.
  const scoped_refptr<render_tree::Typeface>& GetCharacterFallbackTypeface(
      int32 utf32_character, const render_tree::FontStyle& style);

  // Given a string of text, returns the glyph buffer needed to render it.
  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, int32 text_length, bool is_rtl,
      FontList* font_list);

  // Given a string of text, return its width. This is faster than
  // CreateGlyphBuffer().
  float GetTextWidth(const char16* text_buffer, int32 text_length, bool is_rtl,
                     FontList* font_list,
                     render_tree::FontVector* maybe_used_fonts);

 private:
  render_tree::ResourceProvider* resource_provider() const {
    return *resource_provider_;
  }

  void ProcessInactiveFontLists(const base::TimeTicks& current_time);
  void ProcessInactiveFonts(const base::TimeTicks& current_time);

  // Looks up and returns the cached typeface in |local_typeface_map_|. If it
  // doesn't already exist in the cache, then the passed in typeface is added to
  // the cache.
  const scoped_refptr<render_tree::Typeface>& GetCachedLocalTypeface(
      const scoped_refptr<render_tree::Typeface>& typeface);

  // Returns the font if it is in the remote typeface cache and available;
  // otherwise returns NULL.
  // If the font is in the cache, but is not loaded, this call triggers an
  // asynchronous load of it. Both an external load even callback, provided by
  // the constructor and an |OnRemoteFontLoadEvent| callback provided by the
  // font are registered with the remote typeface cache to be called when the
  // load finishes.
  scoped_refptr<render_tree::Font> TryGetRemoteFont(const GURL& url, float size,
                                                    FontListFont::State* state);

  // Returns NULL if the requested family is not empty and is not available in
  // the resource provider. Otherwise, returns the best matching local font.
  scoped_refptr<render_tree::Font> TryGetLocalFont(const std::string& family,
                                                   render_tree::FontStyle style,
                                                   float size,
                                                   FontListFont::State* state);

  // Lookup by a typeface (aka font_face), typeface is defined as font family +
  // style (weight, width, and style).
  // Returns NULL if the requested font face is not found.
  scoped_refptr<render_tree::Font> TryGetLocalFontByFaceName(
      const std::string& font_face, float size, FontListFont::State* state);

  // Called when a remote typeface either successfully loads or fails to load.
  // In either case, the event can impact the fonts contained within the font
  // lists. As a result, the font lists need to have their loading fonts reset
  // so that they'll be re-requested from the cache.
  void OnRemoteTypefaceLoadEvent(const GURL& url);

  render_tree::ResourceProvider** resource_provider_;

  // TODO: Explore eliminating the remote typeface cache and moving its
  // logic into the font cache when the loader interface improves.
  loader::font::RemoteTypefaceCache* const remote_typeface_cache_;
  const base::Closure external_typeface_load_event_callback_;
  const std::string language_script_;

  // Font-face related
  // The cache contains a map of font faces and handles requesting typefaces by
  // url on demand from |remote_typeface_cache_| with a load event callback
  // provided by the constructor. Cached remote typeface returned by
  // |remote_typeface_cache_| have a reference retained by the cache for as long
  // as the cache contains a font face with the corresponding url, to ensure
  // that they remain in memory.
  scoped_ptr<FontFaceMap> font_face_map_;
  RequestedRemoteTypefaceMap requested_remote_typeface_cache_;

  // Font list related
  // This maps unique font property combinations that are currently in use
  // within the document to the font lists that provides the functionality
  // associated with those font property combinations.
  FontListMap font_list_map_;

  // Typeface/Font related
  // Maps of the local typefaces and fonts currently cached. These are used so
  // that the same typefaces and fonts can be shared across multiple font lists,
  // thereby also sharing the internal caching within typeface and font objects.
  // NOTE: Remote typefaces are not cached in |local_typeface_map_|, as the
  // RemoteTypefaceCache handles caching of them.
  TypefaceMap local_typeface_map_;
  FontMap font_map_;
  InactiveFontSet inactive_font_set_;

  // Fallback font related
  // Contains maps of both the unique typeface to use with a specific
  // character-style combination and a prototype font for the typeface, which
  // can be used to provide copies of the font at any desired size, without
  // requiring an additional request of the resource provider for each newly
  // encountered size.
  CharacterFallbackTypefaceMaps character_fallback_typeface_maps_;

  // The last time the cache was checked for inactivity.
  base::TimeTicks last_inactive_process_time_;

  // Thread checker used to verify safe thread usage of the font cache.
  base::ThreadChecker thread_checker_;

  // Font cache's corresponding document's location object. It can provide
  // document's origin.
  scoped_refptr<Location> document_location_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_FONT_CACHE_H_
