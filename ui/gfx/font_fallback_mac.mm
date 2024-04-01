// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

#include "base/i18n/char_iterator.h"
#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback_skia_impl.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

namespace {

bool TextSequenceHasEmoji(base::StringPiece16 text) {
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    const UChar32 codepoint = iter.get();
    if (u_hasBinaryProperty(codepoint, UCHAR_EMOJI))
      return true;
  }
  return false;
}

}  // namespace

std::vector<Font> GetFallbackFonts(const Font& font) {
  DCHECK(font.GetNativeFont());
  // On Mac "There is a system default cascade list (which is polymorphic, based
  // on the user's language setting and current font)" - CoreText Programming
  // Guide.
  NSArray* languages = [[NSUserDefaults standardUserDefaults]
      stringArrayForKey:@"AppleLanguages"];
  CFArrayRef languages_cf = base::mac::NSToCFCast(languages);
  base::ScopedCFTypeRef<CFArrayRef> cascade_list(
      CTFontCopyDefaultCascadeListForLanguages(
          static_cast<CTFontRef>(font.GetNativeFont()), languages_cf));

  std::vector<Font> fallback_fonts;
  if (!cascade_list)
    return fallback_fonts;  // This should only happen for an invalid |font|.

  const CFIndex fallback_count = CFArrayGetCount(cascade_list);
  for (CFIndex i = 0; i < fallback_count; ++i) {
    CTFontDescriptorRef descriptor =
        base::mac::CFCastStrict<CTFontDescriptorRef>(
            CFArrayGetValueAtIndex(cascade_list, i));
    base::ScopedCFTypeRef<CTFontRef> fallback_font(
        CTFontCreateWithFontDescriptor(descriptor, 0.0, nullptr));
    if (fallback_font.get())
      fallback_fonts.push_back(Font(static_cast<NSFont*>(fallback_font.get())));
  }

  if (fallback_fonts.empty())
    return std::vector<Font>(1, font);

  return fallback_fonts;
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  if (TextSequenceHasEmoji(text)) {
    *result = Font("Apple Color Emoji", font.GetFontSize());
    return true;
  }

  sk_sp<SkTypeface> fallback_typeface =
      GetSkiaFallbackTypeface(font, locale, text);

  if (!fallback_typeface)
    return false;

  // Fallback needs to keep the exact SkTypeface, as re-matching the font using
  // family name and styling information loses access to the underlying platform
  // font handles and is not guaranteed to result in the correct typeface, see
  // https://crbug.com/1003829
  *result = Font(PlatformFont::CreateFromSkTypeface(
      std::move(fallback_typeface), font.GetFontSize(), absl::nullopt));
  return true;
}

}  // namespace gfx
