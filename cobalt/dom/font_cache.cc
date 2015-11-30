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

#include "cobalt/dom/csp_delegate.h"

namespace cobalt {
namespace dom {

FontCache::FontCache(render_tree::ResourceProvider* resource_provider,
                     loader::font::RemoteFontCache* remote_font_cache,
                     const base::Closure& external_font_load_event_callback,
                     const CSPDelegate* csp_delegate,
                     const std::string& language)
    : resource_provider_(resource_provider),
      remote_font_cache_(remote_font_cache),
      external_font_load_event_callback_(external_font_load_event_callback),
      csp_delegate_(csp_delegate),
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
  FontUrlSet new_url_set;
  for (FontFaceMap::iterator map_iterator = font_face_map_->begin();
       map_iterator != font_face_map_->end(); ++map_iterator) {
    new_url_set.insert(map_iterator->second);
  }

  // Iterate through active remote font references, verifying that the font face
  // map still contains each remote font's url. Any remote fonts with a url that
  // is no longer contained within |font_face_map_| is purged to allow the
  // remote cache to release the memory.
  loader::font::CachedRemoteFontReferenceVector::iterator reference_iterator =
      remote_font_reference_cache_.begin();
  while (reference_iterator != remote_font_reference_cache_.end()) {
    loader::font::CachedRemoteFontReferenceVector::iterator next_iterator =
        reference_iterator + 1;

    const GURL& url = (*reference_iterator)->cached_resource()->url();

    // If the referenced url is not in the new url set, then purge it.
    if (new_url_set.find(url) == new_url_set.end()) {
      remote_font_reference_cache_.erase(reference_iterator);
      remote_font_reference_cache_urls_.erase(url);
    }

    reference_iterator = next_iterator;
  }
}

FontCache::FontCharacterMap* FontCache::GetFontCharacterMap(
    scoped_refptr<render_tree::Font> font) {
  return &(typeface_id_to_character_map_[font->GetTypefaceId()]);
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
    bool* is_font_loading) {
  FontFaceMap::iterator font_face_entry = font_face_map_->find(family);
  if (font_face_entry != font_face_map_->end()) {
    return TryGetRemoteFont(font_face_entry->second, size, is_font_loading);
  } else {
    return TryGetLocalFont(family, style, size, is_font_loading);
  }
}

scoped_refptr<render_tree::Font> FontCache::TryGetRemoteFont(
    const GURL& url, float size, bool* is_font_loading) {
  // If the CSP delegate does not allow fonts from this source, immediately
  // return NULL. It's a security risk to attempt to load a font from
  // this URL.
  if (!csp_delegate_->CanLoadFont(url)) {
    return NULL;
  }

  // Retrieve the font from the remote font cache, potentially triggering a
  // load.
  scoped_refptr<loader::font::CachedRemoteFont> cached_remote_font =
      remote_font_cache_->CreateCachedResource(url);

  // If the requested url is not currently cached, then create a cached
  // reference, providing callbacks for when the load is completed.
  if (remote_font_reference_cache_urls_.find(url) ==
      remote_font_reference_cache_urls_.end()) {
    // Add the external font load event callback.
    remote_font_reference_cache_.push_back(
        new loader::font::CachedRemoteFontReferenceWithCallbacks(
            cached_remote_font, external_font_load_event_callback_,
            external_font_load_event_callback_));

    // Add the cache's remote font load event callback.
    base::Closure font_cache_callback =
        base::Bind(&FontCache::OnRemoteFontLoadEvent, base::Unretained(this));
    remote_font_reference_cache_.push_back(
        new loader::font::CachedRemoteFontReferenceWithCallbacks(
            cached_remote_font, font_cache_callback, font_cache_callback));

    remote_font_reference_cache_urls_.insert(url);
  }

  *is_font_loading = cached_remote_font->IsLoading();

  scoped_refptr<render_tree::Font> font = cached_remote_font->TryGetResource();
  if (font != NULL) {
    return font->CloneWithSize(size);
  } else {
    return NULL;
  }
}

scoped_refptr<render_tree::Font> FontCache::TryGetLocalFont(
    const std::string& family, render_tree::FontStyle style, float size,
    bool* is_font_loading) {
  // Local fonts are never asynchronously loaded.
  *is_font_loading = false;

  // Only request the local font from the resource provider if the family is
  // empty or the resource provider actually has the family. The reason for this
  // is that the resource provider's |GetLocalFont()| is guaranteed to return a
  // non-NULL value, and in the case where a family is not supported, the
  // subsequent fonts in the font list need to be attempted. An empty family
  // signifies using the default font.
  if (!family.empty() &&
      !resource_provider_->HasLocalFontFamily(family.c_str())) {
    return NULL;
  } else {
    return resource_provider_->GetLocalFont(family.c_str(), style, size);
  }
}

void FontCache::OnRemoteFontLoadEvent() {
  for (FontListMap::iterator font_list_iterator = font_list_map.begin();
       font_list_iterator != font_list_map.end(); ++font_list_iterator) {
    font_list_iterator->second->ResetLoadingFonts();
  }
}

}  // namespace dom
}  // namespace cobalt
