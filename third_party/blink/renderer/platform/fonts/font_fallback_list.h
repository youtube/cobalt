/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_LIST_H_

#include "third_party/blink/renderer/platform/fonts/fallback_list_composite_key.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class FontDescription;
class FontFallbackMap;

const int kCAllFamiliesScanned = -1;

// FontFallbackList caches FontData from FontSelector and FontCache. If font
// updates occur (e.g., @font-face rule changes, web font is loaded, etc.),
// the cached data becomes stale and hence, invalid.
class PLATFORM_EXPORT FontFallbackList
    : public GarbageCollected<FontFallbackList> {
 public:
  explicit FontFallbackList(FontSelector* font_selector);

  FontFallbackList(const FontFallbackList&) = delete;
  FontFallbackList& operator=(const FontFallbackList&) = delete;

  void Trace(Visitor*) const;

  // Returns whether the cached data is valid. We can use a FontFallbackList
  // only when it's valid.
  bool IsValid() const { return !is_invalid_; }

  // Called when font updates (see class comment) have made the cached data
  // invalid. Once marked, a Font object cannot reuse |this|, but have to work
  // on a new instance obtained from FontFallbackMap.
  void MarkInvalid() {
    is_invalid_ = true;
  }

  bool ShouldSkipDrawing() const;

  FontSelector* GetFontSelector() const { return font_selector_.Get(); }
  uint16_t Generation() const { return generation_; }

  NGShapeCache& GetNGShapeCache(const FontDescription& font_description) {
    if (!ng_shape_cache_) {
      ng_shape_cache_ = MakeGarbageCollected<NGShapeCache>();
    }
    return *ng_shape_cache_;
  }

  ShapeCache* GetShapeCache(const FontDescription& font_description) {
    if (!shape_cache_) {
      FallbackListCompositeKey key(font_description);
      shape_cache_ = FontCache::Get().GetShapeCache(key);
    }
    if (font_selector_) {
      shape_cache_->ClearIfVersionChanged(font_selector_->Version());
    }
    return shape_cache_.Get();
  }

  const SimpleFontData* PrimarySimpleFontData(
      const FontDescription& font_description) {
    if (nullify_primary_font_data_for_test_) {
      return nullptr;
    }
    if (!cached_primary_simple_font_data_) {
      cached_primary_simple_font_data_ =
          DeterminePrimarySimpleFontData(font_description);
      DCHECK(cached_primary_simple_font_data_);
    }
    return cached_primary_simple_font_data_;
  }
  const FontData* FontDataAt(const FontDescription&, unsigned index);

  bool CanShapeWordByWord(const FontDescription&);

  void SetCanShapeWordByWordForTesting(bool b) {
    can_shape_word_by_word_ = b;
    can_shape_word_by_word_computed_ = true;
  }

  // See Font::NullifyPrimaryFontForTesting.
  void NullifyPrimarySimpleFontDataForTesting() {
    nullify_primary_font_data_for_test_ = true;
  }

  bool HasLoadingFallback() const { return has_loading_fallback_; }
  bool HasCustomFont() const { return has_custom_font_; }

 private:
  const FontData* GetFontData(const FontDescription&);

  const SimpleFontData* DeterminePrimarySimpleFontData(const FontDescription&);
  const SimpleFontData* DeterminePrimarySimpleFontDataCore(
      const FontDescription&);

  bool ComputeCanShapeWordByWord(const FontDescription&);

  HeapVector<Member<const FontData>, 1> font_list_;
  Member<const SimpleFontData> cached_primary_simple_font_data_ = nullptr;
  const Member<FontSelector> font_selector_;
  int family_index_ = 0;
  const uint16_t generation_;
  bool has_loading_fallback_ : 1;
  bool has_custom_font_ : 1;
  bool can_shape_word_by_word_ : 1;
  bool can_shape_word_by_word_computed_ : 1;
  bool is_invalid_ : 1;
  bool nullify_primary_font_data_for_test_ : 1;

  Member<NGShapeCache> ng_shape_cache_;
  Member<ShapeCache> shape_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_FALLBACK_LIST_H_
