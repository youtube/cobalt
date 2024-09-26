/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_H_

#include <limits.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/fonts/fallback_list_composite_key.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_data_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/font_fallback_linux.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/win/fallback_family_style_cache_win.h"
#endif

class SkString;
class SkTypeface;

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event
}  // namespace base

namespace blink {

class FontDescription;
class FontFaceCreationParams;
class FontFallbackMap;
class FontGlobalContext;
class FontPlatformData;
class FontPlatformDataCache;
class SimpleFontData;
class WebFontPrewarmer;

enum class AlternateFontName {
  kAllowAlternate,
  kNoAlternate,
  kLocalUniqueFace,
  kLastResort
};

typedef HashMap<FallbackListCompositeKey,
                std::unique_ptr<NGShapeCache>,
                FallbackListCompositeKeyTraits>
    FallbackListNGShaperCache;

typedef HashMap<FallbackListCompositeKey,
                std::unique_ptr<ShapeCache>,
                FallbackListCompositeKeyTraits>
    FallbackListShaperCache;

// "und-Zsye", the special locale for retrieving the color emoji font defined
// in UTS #51: https://unicode.org/reports/tr51/#Emoji_Script
extern const char kColorEmojiLocale[];

#if BUILDFLAG(IS_ANDROID)
extern const char kNotoColorEmojiCompat[];
#endif

class PLATFORM_EXPORT FontCache final {
  friend class FontCachePurgePreventer;

  USING_FAST_MALLOC(FontCache);

 public:
  // FontCache initialisation on Windows depends on a global FontMgr being
  // configured through a call from the browser process. CreateIfNeeded helps
  // avoid early creation of a font cache when these globals have not yet
  // been set.
  static FontCache& Get();

  void ReleaseFontData(const SimpleFontData*);

  scoped_refptr<SimpleFontData> FallbackFontForCharacter(
      const FontDescription&,
      UChar32,
      const SimpleFontData* font_data_to_substitute,
      FontFallbackPriority = FontFallbackPriority::kText);

  // Also implemented by the platform.
  void PlatformInit();

  scoped_refptr<SimpleFontData> GetFontData(
      const FontDescription&,
      const AtomicString&,
      AlternateFontName = AlternateFontName::kAllowAlternate,
      ShouldRetain = kRetain);
  scoped_refptr<SimpleFontData> GetLastResortFallbackFont(const FontDescription&,
                                                   ShouldRetain = kRetain);
  SimpleFontData* GetNonRetainedLastResortFallbackFont(const FontDescription&);

  // Should be used in determining whether family names listed in font-family:
  // ... are available locally. Only returns true if family name matches.
  bool IsPlatformFamilyMatchAvailable(const FontDescription&,
                                      const AtomicString& family);

  // Should be used in determining whether the <abc> argument to local in
  // @font-face { ... src: local(<abc>) } are available locally, which should
  // match Postscript name or full font name. Compare
  // https://drafts.csswg.org/css-fonts-3/#src-desc
  // TODO crbug.com/627143 complete this and actually look at the right
  // namerecords.
  bool IsPlatformFontUniqueNameMatchAvailable(
      const FontDescription&,
      const AtomicString& unique_font_name);

  static String FirstAvailableOrFirst(const String&);

  // Returns the NGShapeCache instance associated with the given cache key.
  // Creates a new instance as needed and as such is guaranteed not to return
  // a nullptr. Instances are managed by FontCache and are only guaranteed to
  // be valid for the duration of the current session, as controlled by
  // disable/enablePurging.
  NGShapeCache* GetNGShapeCache(const FallbackListCompositeKey&);

  // Returns the ShapeCache instance associated with the given cache key.
  // Creates a new instance as needed and as such is guaranteed not to return
  // a nullptr. Instances are managed by FontCache and are only guaranteed to
  // be valid for the duration of the current session, as controlled by
  // disable/enablePurging.
  ShapeCache* GetShapeCache(const FallbackListCompositeKey&);

  void AddClient(FontCacheClient*);

  uint16_t Generation();
  void Invalidate();

  sk_sp<SkFontMgr> FontManager() { return font_manager_; }
  static void SetFontManager(sk_sp<SkFontMgr>);

#if BUILDFLAG(IS_WIN)
  static WebFontPrewarmer* GetFontPrewarmer() { return prewarmer_; }
  static void SetFontPrewarmer(WebFontPrewarmer* prewarmer) {
    prewarmer_ = prewarmer;
  }
  static void PrewarmFamily(const AtomicString& family_name);
#else
  static void PrewarmFamily(const AtomicString& family_name) {}
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // These are needed for calling QueryRenderStyleForStrike, since
  // gfx::GetFontRenderParams makes distinctions based on DSF.
  static float DeviceScaleFactor() { return device_scale_factor_; }
  static void SetDeviceScaleFactor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }
#endif

#if !BUILDFLAG(IS_MAC)
  static const AtomicString& SystemFontFamily();
#else
  static const AtomicString& LegacySystemFontFamily();
  static void InvalidateFromAnyThread();
#endif

#if !BUILDFLAG(IS_MAC)
  static void SetSystemFontFamily(const AtomicString&);
#endif

#if BUILDFLAG(IS_WIN)
  // TODO(https://crbug.com/808221) System font style configuration is not
  // related to FontCache. Move it somewhere else, e.g. to WebThemeEngine.
  static bool AntialiasedTextEnabled() { return antialiased_text_enabled_; }
  static bool LcdTextEnabled() { return lcd_text_enabled_; }
  static void SetAntialiasedTextEnabled(bool enabled) {
    antialiased_text_enabled_ = enabled;
  }
  static void SetLCDTextEnabled(bool enabled) { lcd_text_enabled_ = enabled; }
  // Functions to cache and retrieve the system font metrics.
  static void SetMenuFontMetrics(const AtomicString& family_name,
                                 int32_t font_height);
  static void SetSmallCaptionFontMetrics(const AtomicString& family_name,
                                         int32_t font_height);
  static void SetStatusFontMetrics(const AtomicString& family_name,
                                   int32_t font_height);
  static int32_t MenuFontHeight() { return menu_font_height_; }
  static const AtomicString& MenuFontFamily() {
    return *menu_font_family_name_;
  }
  static int32_t SmallCaptionFontHeight() { return small_caption_font_height_; }
  static const AtomicString& SmallCaptionFontFamily() {
    return *small_caption_font_family_name_;
  }
  static int32_t StatusFontHeight() { return status_font_height_; }
  static const AtomicString& StatusFontFamily() {
    return *status_font_family_name_;
  }
  static void SetUseSkiaFontFallback(bool use_skia_font_fallback) {
    use_skia_font_fallback_ = use_skia_font_fallback;
  }

  // On Windows pre 8.1 establish a connection to the DWriteFontProxy service in
  // order to retrieve family names for fallback lookup.
  void EnsureServiceConnected();

  scoped_refptr<SimpleFontData> GetFallbackFamilyNameFromHardcodedChoices(
      const FontDescription&,
      UChar32 codepoint,
      FontFallbackPriority fallback_priority);

  scoped_refptr<SimpleFontData> GetDWriteFallbackFamily(
      const FontDescription&,
      UChar32 codepoint,
      FontFallbackPriority fallback_priority);
#endif  // BUILDFLAG(IS_WIN)

  static void AcceptLanguagesChanged(const String&);

#if BUILDFLAG(IS_ANDROID)
  static AtomicString GetGenericFamilyNameForScript(
      const AtomicString& family_name,
      const AtomicString& generic_family_name_fallback,
      const FontDescription&);
  // Locale-specific families can use different |SkTypeface| for a family name
  // if locale is different.
  static const char* GetLocaleSpecificFamilyName(
      const AtomicString& family_name);
  sk_sp<SkTypeface> CreateLocaleSpecificTypeface(
      const FontDescription& font_description,
      const char* locale_family_name);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static bool GetFontForCharacter(UChar32,
                                  const char* preferred_locale,
                                  gfx::FallbackFontData*);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  scoped_refptr<SimpleFontData> FontDataFromFontPlatformData(
      const FontPlatformData*,
      ShouldRetain = kRetain,
      bool subpixel_ascent_descent = false);

  void InvalidateNGShapeCache();
  void InvalidateShapeCache();

  static void CrashWithFontInfo(const FontDescription*);

  // Memory reporting
  void DumpFontPlatformDataCache(base::trace_event::ProcessMemoryDump*);
  void DumpShapeResultCache(base::trace_event::ProcessMemoryDump*);

  FontFallbackMap& GetFontFallbackMap();

  FontCache(const FontCache&) = delete;
  FontCache& operator=(const FontCache&) = delete;
  ~FontCache();

 private:
  // BCP47 list used when requesting fallback font for a character.
  // inlineCapacity is set to 4: the array vector not need to hold more than 4
  // elements.
  using Bcp47Vector = WTF::Vector<const char*, 4>;

  scoped_refptr<SimpleFontData> PlatformFallbackFontForCharacter(
      const FontDescription&,
      UChar32,
      const SimpleFontData* font_data_to_substitute,
      FontFallbackPriority = FontFallbackPriority::kText);
  sk_sp<SkTypeface> CreateTypefaceFromUniqueName(
      const FontFaceCreationParams& creation_params);

  static Bcp47Vector GetBcp47LocaleForRequest(
      const FontDescription& font_description,
      FontFallbackPriority fallback_priority);

  friend class FontGlobalContext;
  FontCache();

  void Purge(PurgeSeverity = kPurgeIfNeeded);

  void DisablePurging() { purge_prevent_count_++; }
  void EnablePurging() {
    DCHECK(purge_prevent_count_);
    if (!--purge_prevent_count_)
      Purge(kPurgeIfNeeded);
  }

  // FIXME: This method should eventually be removed.
  FontPlatformData* GetFontPlatformData(
      const FontDescription&,
      const FontFaceCreationParams&,
      AlternateFontName = AlternateFontName::kAllowAlternate);
#if !BUILDFLAG(IS_MAC)
  FontPlatformData* SystemFontPlatformData(const FontDescription&);
#endif  // !BUILDFLAG(IS_MAC)

  // These methods are implemented by each platform.
  std::unique_ptr<FontPlatformData> CreateFontPlatformData(
      const FontDescription&,
      const FontFaceCreationParams&,
      float font_size,
      AlternateFontName = AlternateFontName::kAllowAlternate);
  std::unique_ptr<FontPlatformData> ScaleFontPlatformData(
      const FontPlatformData&,
      const FontDescription&,
      const FontFaceCreationParams&,
      float font_size);

  sk_sp<SkTypeface> CreateTypeface(const FontDescription&,
                                   const FontFaceCreationParams&,
                                   std::string& name);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static AtomicString GetFamilyNameForCharacter(SkFontMgr*,
                                                UChar32,
                                                const FontDescription&,
                                                const char* family_name,
                                                FontFallbackPriority);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  scoped_refptr<SimpleFontData> FallbackOnStandardFontStyle(
      const FontDescription&,
      UChar32);

  // Don't purge if this count is > 0;
  int purge_prevent_count_ = 0;

  sk_sp<SkFontMgr> font_manager_;

  // A leaky owning bare pointer.
  static SkFontMgr* static_font_manager_;

#if BUILDFLAG(IS_WIN)
  static WebFontPrewarmer* prewarmer_;
  static bool antialiased_text_enabled_;
  static bool lcd_text_enabled_;
  // The system font metrics cache.
  static AtomicString* menu_font_family_name_;
  static int32_t menu_font_height_;
  static AtomicString* small_caption_font_family_name_;
  static int32_t small_caption_font_height_;
  static AtomicString* status_font_family_name_;
  static int32_t status_font_height_;
  static bool use_skia_font_fallback_;

  // Windows creates an SkFontMgr for unit testing automatically. This flag is
  // to ensure it's not happening in the production from the crash log.
  bool is_test_font_mgr_ = false;
  mojo::Remote<mojom::blink::DWriteFontProxy> service_;
  std::unique_ptr<FallbackFamilyStyleCache> fallback_params_cache_;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static float device_scale_factor_;
#endif

  uint16_t generation_ = 0;
  bool platform_init_ = false;
  Persistent<HeapHashSet<WeakMember<FontCacheClient>>> font_cache_clients_;
  std::unique_ptr<FontPlatformDataCache> font_platform_data_cache_;
  absl::optional<FallbackListNGShaperCache> fallback_list_ng_shaper_cache_;
  FallbackListShaperCache fallback_list_shaper_cache_;

  std::unique_ptr<FontDataCache> font_data_cache_;

  Persistent<FontFallbackMap> font_fallback_map_;

  void PurgePlatformFontDataCache();
  void PurgeFallbackListNGShaperCache();
  void PurgeFallbackListShaperCache();

  friend class SimpleFontData;  // For fontDataFromFontPlatformData
  friend class FontFallbackList;
  friend class FontPlatformDataCache;
  FRIEND_TEST_ALL_PREFIXES(FontCacheAndroidTest, LocaleSpecificTypeface);
};

class PLATFORM_EXPORT FontCachePurgePreventer {
  USING_FAST_MALLOC(FontCachePurgePreventer);

 public:
  FontCachePurgePreventer() { FontCache::Get().DisablePurging(); }
  FontCachePurgePreventer(const FontCachePurgePreventer&) = delete;
  FontCachePurgePreventer& operator=(const FontCachePurgePreventer&) = delete;
  ~FontCachePurgePreventer() { FontCache::Get().EnablePurging(); }
};

AtomicString ToAtomicString(const SkString&);

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/1241875) Can this be simplified?
// static
inline const char* FontCache::GetLocaleSpecificFamilyName(
    const AtomicString& family_name) {
  // Only `serif` has `fallbackFor` according to the current `fonts.xml`.
  if (family_name == font_family_names::kSerif)
    return "serif";
  return nullptr;
}
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CACHE_H_
