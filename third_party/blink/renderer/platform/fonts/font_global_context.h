// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_

#include "base/containers/lru_cache.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

class FontCache;
class FontUniqueNameLookup;
class HarfBuzzFontCache;

// FontGlobalContext contains non-thread-safe, thread-specific data used for
// font formatting.
class PLATFORM_EXPORT FontGlobalContext {
  USING_FAST_MALLOC(FontGlobalContext);

 public:
  ~FontGlobalContext();

  static FontGlobalContext& Get();
  static FontGlobalContext* TryGet();

  FontGlobalContext(const FontGlobalContext&) = delete;
  FontGlobalContext& operator=(const FontGlobalContext&) = delete;

  static inline FontCache& GetFontCache() { return Get().font_cache_; }

  static HarfBuzzFontCache& GetHarfBuzzFontCache();

  static FontUniqueNameLookup* GetFontUniqueNameLookup();

  IdentifiableToken GetOrComputeTypefaceDigest(const FontPlatformData& source);
  IdentifiableToken GetOrComputePostScriptNameDigest(
      const FontPlatformData& source);

  // Called by MemoryPressureListenerRegistry to clear memory.
  static void ClearMemory();

  // |Init()| should be called in main thread.
  static void Init();

 private:
  FontGlobalContext();

  FontCache font_cache_;
  std::unique_ptr<HarfBuzzFontCache> harfbuzz_font_cache_;
  std::unique_ptr<FontUniqueNameLookup> font_unique_name_lookup_;
  base::HashingLRUCache<SkFontID, IdentifiableToken> typeface_digest_cache_;
  base::HashingLRUCache<SkFontID, IdentifiableToken>
      postscript_name_digest_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_GLOBAL_CONTEXT_H_
