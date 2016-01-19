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

#include "cobalt/dom/font_cache.h"

namespace cobalt {
namespace dom {

FontCache::RequestedRemoteFontInfo::RequestedRemoteFontInfo(
    const scoped_refptr<loader::font::CachedRemoteFont>& cached_remote_font,
    const base::Closure& font_load_event_callback)
    : cached_remote_font_reference_(
          new loader::font::CachedRemoteFontReferenceWithCallbacks(
              cached_remote_font, font_load_event_callback,
              font_load_event_callback)),
      request_timer_(new base::Timer(false, false)) {
  request_timer_->Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kRequestTimerDelay),
                        font_load_event_callback);
}

FontCache::FontCache(render_tree::ResourceProvider* resource_provider,
                     loader::font::RemoteFontCache* remote_font_cache,
                     const base::Closure& external_font_load_event_callback,
                     const std::string& language)
    : resource_provider_(resource_provider),
      remote_font_cache_(remote_font_cache),
      external_font_load_event_callback_(external_font_load_event_callback),
      language_(language),
      font_face_map_(new FontFaceMap()) {}

scoped_refptr<dom::FontList> FontCache::GetFontList(
    const FontListKey& font_list_key) {
  FontListMap::iterator list_iterator = font_list_map.find(font_list_key);
  if (list_iterator != font_list_map.end()) {
    return list_iterator->second;
  } else {
    // If the font list isn't already in the map, both set the new font list
    // mapping and return the newly created font list.
    return font_list_map[font_list_key] = new FontList(this, font_list_key);
  }
}

void FontCache::RemoveUnusedFontLists() {
  for (FontListMap::iterator font_list_iterator = font_list_map.begin();
       font_list_iterator != font_list_map.end();) {
    // Any font list that has a single ref is unreferenced outside of the font
    // cache and is no longer used. Remove it.
    if (font_list_iterator->second->HasOneRef()) {
      font_list_map.erase(font_list_iterator++);
    } else {
      ++font_list_iterator;
    }
  }
}

void FontCache::SetFontFaceMap(scoped_ptr<FontFaceMap> font_face_map) {
  // If nothing has changed, then there's nothing to update. Just return.
  if (*font_face_map == *font_face_map_) {
    return;
  }

  // Clear out the cached font lists. It may no longer contain valid font
  // mappings as a result of the font face map changing.
  font_list_map.clear();

  font_face_map_ = font_face_map.Pass();

  // Generate a set of the urls contained within the new font face map.
  std::set<GURL> new_url_set;
  for (FontFaceMap::iterator map_iterator = font_face_map_->begin();
       map_iterator != font_face_map_->end(); ++map_iterator) {
    map_iterator->second.CollectUrlSources(&new_url_set);
  }

  // Iterate through active remote font references, verifying that the font face
  // map still contains each remote font's url. Any remote fonts with a url that
  // is no longer contained within |font_face_map_| is purged to allow the
  // remote cache to release the memory.
  RequestedRemoteFontMap::iterator requested_remote_font_iterator =
      requested_remote_font_cache_.begin();
  while (requested_remote_font_iterator != requested_remote_font_cache_.end()) {
    RequestedRemoteFontMap::iterator current_iterator =
        requested_remote_font_iterator++;

    // If the referenced url is not in the new url set, then purge it.
    if (new_url_set.find(current_iterator->first) == new_url_set.end()) {
      requested_remote_font_cache_.erase(current_iterator);
    }
  }
}

FontCache::FontCharacterGlyphMap* FontCache::GetFontCharacterGlyphMap(
    scoped_refptr<render_tree::Font> font) {
  return &(typeface_id_to_character_glyph_map_[font->GetTypefaceId()]);
}

render_tree::TypefaceId FontCache::GetCharacterFallbackTypefaceId(
    int32 utf32_character, render_tree::FontStyle style, float size) {
  CharacterFallbackTypefaceKey fallback_key(utf32_character, style);

  // Grab the fallback character map for the specified style and attempt to
  // lookup the character. If it's already in the map, just return the
  // associated typeface id.
  CharacterFallbackTypefaceMap::iterator fallback_iterator =
      character_fallback_typeface_map_.find(fallback_key);
  if (fallback_iterator != character_fallback_typeface_map_.end()) {
    return fallback_iterator->second;
  }

  // Otherwise, the character didn't already exist in the map. Grab the fallback
  // font for the character and style from the resource provider.
  scoped_refptr<render_tree::Font> fallback_font =
      resource_provider_->GetCharacterFallbackFont(utf32_character, style, size,
                                                   language_);

  // Map the character to the typeface id of the returned font.
  render_tree::TypefaceId typeface_id = fallback_font->GetTypefaceId();
  character_fallback_typeface_map_[fallback_key] = typeface_id;

  // Check to see if the typeface id already maps to a font. If it doesn't, then
  // add the mapping. After this function is called, the typeface to font
  // mapping is guaranteed to exist.
  FallbackTypefaceToFontMap::iterator fallback_font_iterator =
      fallback_typeface_to_font_map_.find(typeface_id);
  if (fallback_font_iterator == fallback_typeface_to_font_map_.end()) {
    fallback_typeface_to_font_map_[typeface_id] = fallback_font;
  }

  return typeface_id;
}

scoped_refptr<render_tree::Font> FontCache::GetFallbackFont(
    render_tree::TypefaceId typeface_id, float size) {
  FallbackTypefaceToFontMap::iterator fallback_font_iterator =
      fallback_typeface_to_font_map_.find(typeface_id);
  if (fallback_font_iterator != fallback_typeface_to_font_map_.end()) {
    return fallback_font_iterator->second->CloneWithSize(size);
  } else {
    return NULL;
  }
}

scoped_refptr<render_tree::Font> FontCache::TryGetFont(
    const std::string& family, render_tree::FontStyle style, float size,
    FontListFont::State* state) {
  FontFaceMap::iterator font_face_map_iterator = font_face_map_->find(family);
  if (font_face_map_iterator != font_face_map_->end()) {
    // Retrieve the font face style set entry that most closely matches the
    // desired style. Given that a font face was found for this family, it
    // should never be NULL.
    // https://www.w3.org/TR/css3-fonts/#font-prop-desc
    const FontFaceStyleSet::Entry* style_set_entry =
        font_face_map_iterator->second.MatchStyle(style);
    DCHECK(style_set_entry != NULL);

    // Walk the entry's sources:
    // - If a remote source is encountered, always return the results of its
    //   attempted retrieval, regardless of its success.
    // - If a local source is encountered, only return the local font if it is
    //   successfully retrieved. In the case where the font is not locally
    //   available, the next font in the source list should be attempted
    //   instead.
    // https://www.w3.org/TR/css3-fonts/#src-desc
    for (FontFaceSources::const_iterator source_iterator =
             style_set_entry->sources.begin();
         source_iterator != style_set_entry->sources.end(); ++source_iterator) {
      if (source_iterator->IsUrlSource()) {
        return TryGetRemoteFont(source_iterator->GetUrl(), size, state);
      } else {
        scoped_refptr<render_tree::Font> font =
            TryGetLocalFont(source_iterator->GetName(), style, size, state);
        if (font != NULL) {
          return font;
        }
      }
    }

    *state = FontListFont::kUnavailableState;
    return NULL;
  } else {
    return TryGetLocalFont(family, style, size, state);
  }
}

scoped_refptr<render_tree::Font> FontCache::TryGetRemoteFont(
    const GURL& url, float size, FontListFont::State* state) {
  // Retrieve the font from the remote font cache, potentially triggering a
  // load.
  scoped_refptr<loader::font::CachedRemoteFont> cached_remote_font =
      remote_font_cache_->CreateCachedResource(url);

  RequestedRemoteFontMap::iterator requested_remote_font_iterator =
      requested_remote_font_cache_.find(url);

  // If the requested url is not currently cached, then create a cached
  // reference and request timer, providing callbacks for when the load is
  // completed or the timer expires.
  if (requested_remote_font_iterator == requested_remote_font_cache_.end()) {
    // Create the remote font load event's callback. This callback occurs on
    // successful loads, failed loads, and when the request's timer expires.
    base::Closure font_load_event_callback = base::Bind(
        &FontCache::OnRemoteFontLoadEvent, base::Unretained(this), url);

    // Insert the newly requested remote font's info into the cache, and set the
    // iterator from the return value of the map insertion.
    requested_remote_font_iterator =
        requested_remote_font_cache_.insert(
                                        std::make_pair(
                                            url, new RequestedRemoteFontInfo(
                                                     cached_remote_font,
                                                     font_load_event_callback)))
            .first;
  }

  scoped_refptr<render_tree::Font> font = cached_remote_font->TryGetResource();
  if (font != NULL) {
    *state = FontListFont::kLoadedState;
    return font->CloneWithSize(size);
  } else {
    if (cached_remote_font->IsLoading()) {
      if (requested_remote_font_iterator->second->HasActiveRequestTimer()) {
        *state = FontListFont::kLoadingWithTimerActiveState;
      } else {
        *state = FontListFont::kLoadingWithTimerExpiredState;
      }
    } else {
      *state = FontListFont::kUnavailableState;
    }
    return NULL;
  }
}

scoped_refptr<render_tree::Font> FontCache::TryGetLocalFont(
    const std::string& family, render_tree::FontStyle style, float size,
    FontListFont::State* state) {
  // Only request the local font from the resource provider if the family is
  // empty or the resource provider actually has the family. The reason for this
  // is that the resource provider's |GetLocalFont()| is guaranteed to return a
  // non-NULL value, and in the case where a family is not supported, the
  // subsequent fonts in the font list need to be attempted. An empty family
  // signifies using the default font.
  if (!family.empty() &&
      !resource_provider_->HasLocalFontFamily(family.c_str())) {
    *state = FontListFont::kUnavailableState;
    return NULL;
  } else {
    *state = FontListFont::kLoadedState;
    return resource_provider_->GetLocalFont(family.c_str(), style, size);
  }
}

void FontCache::OnRemoteFontLoadEvent(const GURL& url) {
  RequestedRemoteFontMap::iterator requested_remote_font_iterator =
      requested_remote_font_cache_.find(url);
  if (requested_remote_font_iterator != requested_remote_font_cache_.end()) {
    // NOTE(***REMOVED***): We can potentially track the exact font list fonts that are
    // impacted by each load event and only reset them. However, as a result of
    // the minimal amount of processing required to update the loading status of
    // a font, the small number of fonts involved, and the fact that this is an
    // infrequent event, adding this additional layer of tracking complexity
    // doesn't appear to offer any meaningful benefits.
    for (FontListMap::iterator font_list_iterator = font_list_map.begin();
         font_list_iterator != font_list_map.end(); ++font_list_iterator) {
      font_list_iterator->second->ResetLoadingFonts();
    }

    // Clear the request timer. It only runs until the first load event occurs.
    requested_remote_font_iterator->second->ClearRequestTimer();

    external_font_load_event_callback_.Run();
  }
}

}  // namespace dom
}  // namespace cobalt
