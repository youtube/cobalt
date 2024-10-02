/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010, 2012 Google Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/win/font_fallback_win.h"

#include <unicode/uchar.h>

#include <limits>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {

const char kArial[] = "Arial";
const char kCourierNew[] = "Courier New";
const char kTimesNewRoman[] = "Times New Roman";

static inline bool IsFontPresent(const UChar* font_name,
                                 SkFontMgr* font_manager) {
  String family = font_name;
  sk_sp<SkTypeface> tf(
      font_manager->matchFamilyStyle(family.Utf8().c_str(), SkFontStyle()));
  if (!tf)
    return false;

  SkTypeface::LocalizedStrings* actual_families =
      tf->createFamilyNameIterator();
  bool matches_requested_family = false;
  SkTypeface::LocalizedString actual_family;
  while (actual_families->next(&actual_family)) {
    if (DeprecatedEqualIgnoringCase(
            family, AtomicString::FromUTF8(actual_family.fString.c_str()))) {
      matches_requested_family = true;
      break;
    }
  }
  actual_families->unref();

  return matches_requested_family;
}

struct FontMapping {
  const UChar* family_name;
  base::span<const UChar* const> candidate_family_names;
};
// A simple mapping from UScriptCode to family name. This is a sparse array,
// which works well since the range of UScriptCode values is small.
typedef FontMapping ScriptToFontMap[USCRIPT_CODE_LIMIT];

const UChar* FindMonospaceFontForScript(UScriptCode script) {
  struct FontMap {
    UScriptCode script;
    const UChar* family;
  };

  static const FontMap kFontMap[] = {
      {USCRIPT_HEBREW, u"courier new"},
      {USCRIPT_ARABIC, u"courier new"},
  };

  for (const auto& font_family : kFontMap) {
    if (font_family.script == script)
      return font_family.family;
  }
  return nullptr;
}

void InitializeScriptFontMap(ScriptToFontMap& script_font_map) {
  struct ScriptToFontFamilies {
    UScriptCode script;
    base::span<const UChar* const> families;
  };

  // For the following scripts, multiple fonts may be listed. They are tried
  // in order. The first slot is preferred but the font may not be available,
  // if so the remaining slots are tried in order.
  // In general the order is the Windows 10 font follow by the 8.1, 8.0 and
  // finally the font for Windows 7.
  // For scripts where an optional or region specific font may be available
  // that should be listed before the generic one.
  // Based on the "Script and Font Support in Windows" MSDN documentation [1]
  // with overrides and additional fallbacks as needed.
  // 1: https://msdn.microsoft.com/en-us/goglobal/bb688099.aspx
  static const UChar* const kArabicFonts[] = {u"Tahoma", u"Segoe UI"};
  static const UChar* const kArmenianFonts[] = {u"Segoe UI", u"Sylfaen"};
  static const UChar* const kBengaliFonts[] = {u"Nirmala UI", u"Vrinda"};
  static const UChar* const kBrahmiFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kBrailleFonts[] = {u"Segoe UI Symbol"};
  static const UChar* const kBugineseFonts[] = {u"Leelawadee UI"};
  static const UChar* const kCanadianAaboriginalFonts[] = {u"Gadugi",
                                                           u"Euphemia"};
  static const UChar* const kCarianFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kCherokeeFonts[] = {u"Gadugi", u"Plantagenet"};
  static const UChar* const kCopticFonts[] = {u"Segoe UI Symbol"};
  static const UChar* const kCuneiformFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kCypriotFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kCyrillicFonts[] = {u"Times New Roman"};
  static const UChar* const kDeseretFonts[] = {u"Segoe UI Symbol"};
  static const UChar* const kDevanagariFonts[] = {u"Nirmala UI", u"Mangal"};
  static const UChar* const kEgyptianHieroglyphsFonts[] = {
      u"Segoe UI Historic"};
  static const UChar* const kEthiopicFonts[] = {u"Nyala",
                                                u"Abyssinica SIL",
                                                u"Ethiopia Jiret",
                                                u"Visual Geez Unicode",
                                                u"GF Zemen Unicode",
                                                u"Ebrima"};
  static const UChar* const kGeorgianFonts[] = {u"Sylfaen", u"Segoe UI"};
  static const UChar* const kGlagoliticFonts[] = {u"Segoe UI Historic",
                                                  u"Segoe UI Symbol"};
  static const UChar* const kGothicFonts[] = {u"Segoe UI Historic",
                                              u"Segoe UI Symbol"};
  static const UChar* const kGreekFonts[] = {u"Times New Roman"};
  static const UChar* const kGujaratiFonts[] = {u"Nirmala UI", u"Shruti"};
  static const UChar* const kGurmukhiFonts[] = {u"Nirmala UI", u"Raavi"};
  static const UChar* const kHangulFonts[] = {u"Malgun Gothic", u"Gulim"};
  static const UChar* const kHebrewFonts[] = {u"David", u"Segoe UI"};
  static const UChar* const kImperialAramaicFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kInscriptionalPahlaviFonts[] = {
      u"Segoe UI Historic"};
  static const UChar* const kInscriptionalParthianFonts[] = {
      u"Segoe UI Historic"};
  static const UChar* const kJavaneseFonts[] = {u"Javanese Text"};
  static const UChar* const kKannadaFonts[] = {u"Tunga", u"Nirmala UI"};
  static const UChar* const kKatakanaOrHiraganaFonts[] = {
      u"Meiryo", u"Yu Gothic", u"MS PGothic", u"Microsoft YaHei"};
  static const UChar* const kKharoshthiFonts[] = {u"Segoe UI Historic"};
  // Try Khmer OS before Vista fonts as it goes along better with Latin
  // and looks better/larger for the same size.
  static const UChar* const kKhmerFonts[] = {
      u"Leelawadee UI", u"Khmer UI", u"Khmer OS", u"MoolBoran", u"DaunPenh"};
  static const UChar* const kLaoFonts[] = {u"Leelawadee UI", u"Lao UI",
                                           u"DokChampa",     u"Saysettha OT",
                                           u"Phetsarath OT", u"Code2000"};
  static const UChar* const kLatinFonts[] = {u"Times New Roman"};
  static const UChar* const kLisuFonts[] = {u"Segoe UI"};
  static const UChar* const kLycianFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kLydianFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kMalayalamFonts[] = {u"Nirmala UI", u"Kartika"};
  static const UChar* const kMeroiticCursiveFonts[] = {u"Segoe UI Historic",
                                                       u"Segoe UI Symbol"};
  static const UChar* const kMongolianFonts[] = {u"Mongolian Baiti"};
  static const UChar* const kMyanmarFonts[] = {
      u"Myanmar Text", u"Padauk", u"Parabaik", u"Myanmar3", u"Code2000"};
  static const UChar* const kNewTaiLueFonts[] = {u"Microsoft New Tai Lue"};
  static const UChar* const kNkoFonts[] = {u"Ebrima"};
  static const UChar* const kOghamFonts[] = {u"Segoe UI Historic",
                                             u"Segoe UI Symbol"};
  static const UChar* const kOlChikiFonts[] = {u"Nirmala UI"};
  static const UChar* const kOldItalicFonts[] = {u"Segoe UI Historic",
                                                 u"Segoe UI Symbol"};
  static const UChar* const kOldPersianFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kOldSouthArabianFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kOriyaFonts[] = {u"Kalinga", u"ori1Uni",
                                             u"Lohit Oriya", u"Nirmala UI"};
  static const UChar* const kOrkhonFonts[] = {u"Segoe UI Historic",
                                              u"Segoe UI Symbol"};
  static const UChar* const kOsmanyaFonts[] = {u"Ebrima"};
  static const UChar* const kPhagsPaFonts[] = {u"Microsoft PhagsPa"};
  static const UChar* const kRunicFonts[] = {u"Segoe UI Historic",
                                             u"Segoe UI Symbol"};
  static const UChar* const kShavianFonts[] = {u"Segoe UI Historic"};
  static const UChar* const kSimplifiedHanFonts[] = {u"Microsoft YaHei",
                                                     u"simsun"};
  static const UChar* const kSinhalaFonts[] = {u"Iskoola Pota",
                                               u"AksharUnicode", u"Nirmala UI"};
  static const UChar* const kSoraSompengFonts[] = {u"Nirmala UI"};
  static const UChar* const kSymbolsFonts[] = {u"Segoe UI Symbol"};
  static const UChar* const kSyriacFonts[] = {
      u"Estrangelo Edessa", u"Estrangelo Nisibin", u"Code2000"};
  static const UChar* const kTaiLeFonts[] = {u"Microsoft Tai Le"};
  static const UChar* const kTamilFonts[] = {u"Nirmala UI", u"Latha"};
  static const UChar* const kTeluguFonts[] = {u"Nirmala UI", u"Gautami"};
  static const UChar* const kThaanaFonts[] = {u"MV Boli"};
  static const UChar* const kThaiFonts[] = {u"Tahoma", u"Leelawadee UI",
                                            u"Leelawadee"};
  static const UChar* const kTibetanFonts[] = {
      u"Microsoft Himalaya", u"Jomolhari", u"Tibetan Machine Uni"};
  static const UChar* const kTifinaghFonts[] = {u"Ebrima"};
  static const UChar* const kTraditionalHanFonts[] = {u"Microsoft JhengHei",
                                                      u"pmingliu"};
  static const UChar* const kVaiFonts[] = {u"Ebrima"};
  static const UChar* const kYiFonts[] = {u"Microsoft Yi Baiti", u"Nuosu SIL",
                                          u"Code2000"};

  static const ScriptToFontFamilies kScriptToFontFamilies[] = {
      {USCRIPT_ARABIC, kArabicFonts},
      {USCRIPT_ARMENIAN, kArmenianFonts},
      {USCRIPT_BENGALI, kBengaliFonts},
      {USCRIPT_BRAHMI, kBrahmiFonts},
      {USCRIPT_BRAILLE, kBrailleFonts},
      {USCRIPT_BUGINESE, kBugineseFonts},
      {USCRIPT_CANADIAN_ABORIGINAL, kCanadianAaboriginalFonts},
      {USCRIPT_CARIAN, kCarianFonts},
      {USCRIPT_CHEROKEE, kCherokeeFonts},
      {USCRIPT_COPTIC, kCopticFonts},
      {USCRIPT_CUNEIFORM, kCuneiformFonts},
      {USCRIPT_CYPRIOT, kCypriotFonts},
      {USCRIPT_CYRILLIC, kCyrillicFonts},
      {USCRIPT_DESERET, kDeseretFonts},
      {USCRIPT_DEVANAGARI, kDevanagariFonts},
      {USCRIPT_EGYPTIAN_HIEROGLYPHS, kEgyptianHieroglyphsFonts},
      {USCRIPT_ETHIOPIC, kEthiopicFonts},
      {USCRIPT_GEORGIAN, kGeorgianFonts},
      {USCRIPT_GLAGOLITIC, kGlagoliticFonts},
      {USCRIPT_GOTHIC, kGothicFonts},
      {USCRIPT_GREEK, kGreekFonts},
      {USCRIPT_GUJARATI, kGujaratiFonts},
      {USCRIPT_GURMUKHI, kGurmukhiFonts},
      {USCRIPT_HANGUL, kHangulFonts},
      {USCRIPT_HEBREW, kHebrewFonts},
      {USCRIPT_HIRAGANA, kKatakanaOrHiraganaFonts},
      {USCRIPT_IMPERIAL_ARAMAIC, kImperialAramaicFonts},
      {USCRIPT_INSCRIPTIONAL_PAHLAVI, kInscriptionalPahlaviFonts},
      {USCRIPT_INSCRIPTIONAL_PARTHIAN, kInscriptionalParthianFonts},
      {USCRIPT_JAVANESE, kJavaneseFonts},
      {USCRIPT_KANNADA, kKannadaFonts},
      {USCRIPT_KATAKANA, kKatakanaOrHiraganaFonts},
      {USCRIPT_KATAKANA_OR_HIRAGANA, kKatakanaOrHiraganaFonts},
      {USCRIPT_KHAROSHTHI, kKharoshthiFonts},
      {USCRIPT_KHMER, kKhmerFonts},
      {USCRIPT_LAO, kLaoFonts},
      {USCRIPT_LATIN, kLatinFonts},
      {USCRIPT_LISU, kLisuFonts},
      {USCRIPT_LYCIAN, kLycianFonts},
      {USCRIPT_LYDIAN, kLydianFonts},
      {USCRIPT_MALAYALAM, kMalayalamFonts},
      {USCRIPT_MEROITIC_CURSIVE, kMeroiticCursiveFonts},
      {USCRIPT_MONGOLIAN, kMongolianFonts},
      {USCRIPT_MYANMAR, kMyanmarFonts},
      {USCRIPT_NEW_TAI_LUE, kNewTaiLueFonts},
      {USCRIPT_NKO, kNkoFonts},
      {USCRIPT_OGHAM, kOghamFonts},
      {USCRIPT_OL_CHIKI, kOlChikiFonts},
      {USCRIPT_OLD_ITALIC, kOldItalicFonts},
      {USCRIPT_OLD_PERSIAN, kOldPersianFonts},
      {USCRIPT_OLD_SOUTH_ARABIAN, kOldSouthArabianFonts},
      {USCRIPT_ORIYA, kOriyaFonts},
      {USCRIPT_ORKHON, kOrkhonFonts},
      {USCRIPT_OSMANYA, kOsmanyaFonts},
      {USCRIPT_PHAGS_PA, kPhagsPaFonts},
      {USCRIPT_RUNIC, kRunicFonts},
      {USCRIPT_SHAVIAN, kShavianFonts},
      {USCRIPT_SIMPLIFIED_HAN, kSimplifiedHanFonts},
      {USCRIPT_SINHALA, kSinhalaFonts},
      {USCRIPT_SORA_SOMPENG, kSoraSompengFonts},
      {USCRIPT_SYMBOLS, kSymbolsFonts},
      {USCRIPT_SYRIAC, kSyriacFonts},
      {USCRIPT_TAI_LE, kTaiLeFonts},
      {USCRIPT_TAMIL, kTamilFonts},
      {USCRIPT_TELUGU, kTeluguFonts},
      {USCRIPT_THAANA, kThaanaFonts},
      {USCRIPT_THAI, kThaiFonts},
      {USCRIPT_TIBETAN, kTibetanFonts},
      {USCRIPT_TIFINAGH, kTifinaghFonts},
      {USCRIPT_TRADITIONAL_HAN, kTraditionalHanFonts},
      {USCRIPT_VAI, kVaiFonts},
      {USCRIPT_YI, kYiFonts}};

  for (const auto& font_family : kScriptToFontFamilies) {
    script_font_map[font_family.script].candidate_family_names =
        font_family.families;
  }

  // Initialize the locale-dependent mapping from system locale.
  UScriptCode han_script = LayoutLocale::GetSystem().GetScriptForHan();
  DCHECK_NE(han_script, USCRIPT_HAN);
  if (!script_font_map[han_script].candidate_family_names.empty()) {
    script_font_map[USCRIPT_HAN].candidate_family_names =
        script_font_map[han_script].candidate_family_names;
  }
}

void FindFirstExistingCandidateFont(FontMapping& script_font_mapping,
                                    SkFontMgr* font_manager) {
  for (const UChar* family : script_font_mapping.candidate_family_names) {
    if (IsFontPresent(family, font_manager)) {
      script_font_mapping.family_name = family;
      break;
    }
  }
  script_font_mapping.candidate_family_names = {};
}

// There are a lot of characters in USCRIPT_COMMON that can be covered
// by fonts for scripts closely related to them. See
// http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[:Script=Common:]
// FIXME: make this more efficient with a wider coverage
UScriptCode GetScriptBasedOnUnicodeBlock(int ucs4) {
  UBlockCode block = ublock_getCode(ucs4);
  switch (block) {
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
      return USCRIPT_HAN;
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
      return USCRIPT_KATAKANA_OR_HIRAGANA;
    case UBLOCK_ARABIC:
      return USCRIPT_ARABIC;
    case UBLOCK_THAI:
      return USCRIPT_THAI;
    case UBLOCK_GREEK:
      return USCRIPT_GREEK;
    case UBLOCK_DEVANAGARI:
      // For Danda and Double Danda (U+0964, U+0965), use a Devanagari
      // font for now although they're used by other scripts as well.
      // Without a context, we can't do any better.
      return USCRIPT_DEVANAGARI;
    case UBLOCK_ARMENIAN:
      return USCRIPT_ARMENIAN;
    case UBLOCK_GEORGIAN:
      return USCRIPT_GEORGIAN;
    case UBLOCK_KANNADA:
      return USCRIPT_KANNADA;
    case UBLOCK_GOTHIC:
      return USCRIPT_GOTHIC;
    default:
      return USCRIPT_COMMON;
  }
}

UScriptCode GetScript(int ucs4) {
  ICUError err;
  UScriptCode script = uscript_getScript(ucs4, &err);
  // If script is invalid, common or inherited or there's an error,
  // infer a script based on the unicode block of a character.
  if (script <= USCRIPT_INHERITED || U_FAILURE(err))
    script = GetScriptBasedOnUnicodeBlock(ucs4);
  return script;
}

const UChar* GetFontBasedOnUnicodeBlock(UBlockCode block_code,
                                        SkFontMgr* font_manager) {
  static const UChar* const kEmojiFonts[] = {u"Segoe UI Emoji",
                                             u"Segoe UI Symbol"};
  static const UChar* const kMathFonts[] = {u"Cambria Math", u"Segoe UI Symbol",
                                            u"Code2000"};
  static const UChar* const kSymbolFont = u"Segoe UI Symbol";
  static const UChar* emoji_font = 0;
  static const UChar* math_font = 0;
  static bool initialized = false;
  if (!initialized) {
    for (const UChar* font : kEmojiFonts) {
      if (IsFontPresent(font, font_manager)) {
        emoji_font = font;
        break;
      }
    }
    for (const UChar* font : kMathFonts) {
      if (IsFontPresent(font, font_manager)) {
        math_font = font;
        break;
      }
    }
    initialized = true;
  }

  switch (block_code) {
    case UBLOCK_EMOTICONS:
    case UBLOCK_ENCLOSED_ALPHANUMERIC_SUPPLEMENT:
      return emoji_font;
    case UBLOCK_PLAYING_CARDS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_ARROWS:
    case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS:
    case UBLOCK_TRANSPORT_AND_MAP_SYMBOLS:
    case UBLOCK_ALCHEMICAL_SYMBOLS:
    case UBLOCK_DINGBATS:
    case UBLOCK_GOTHIC:
      return kSymbolFont;
    case UBLOCK_ARROWS:
    case UBLOCK_MATHEMATICAL_OPERATORS:
    case UBLOCK_MISCELLANEOUS_TECHNICAL:
    case UBLOCK_GEOMETRIC_SHAPES:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_A:
    case UBLOCK_SUPPLEMENTAL_ARROWS_B:
    case UBLOCK_MISCELLANEOUS_MATHEMATICAL_SYMBOLS_B:
    case UBLOCK_SUPPLEMENTAL_MATHEMATICAL_OPERATORS:
    case UBLOCK_MATHEMATICAL_ALPHANUMERIC_SYMBOLS:
    case UBLOCK_ARABIC_MATHEMATICAL_ALPHABETIC_SYMBOLS:
    case UBLOCK_GEOMETRIC_SHAPES_EXTENDED:
      return math_font;
    default:
      return 0;
  }
}

}  // namespace

// FIXME: this is font fallback code version 0.1
//  - Cover all the scripts
//  - Get the default font for each script/generic family from the
//    preference instead of hardcoding in the source.
//    (at least, read values from the registry for IE font settings).
//  - Support generic families (from FontDescription)
//  - If the default font for a script is not available,
//    try some more fonts known to support it. Finally, we can
//    use EnumFontFamilies or similar APIs to come up with a list of
//    fonts supporting the script and cache the result.
//  - Consider using UnicodeSet (or UnicodeMap) converted from
//    GLYPHSET (BMP) or directly read from truetype cmap tables to
//    keep track of which character is supported by which font
//  - Update script_font_cache in response to WM_FONTCHANGE

const UChar* GetFontFamilyForScript(UScriptCode script,
                                    FontDescription::GenericFamilyType generic,
                                    SkFontMgr* font_manager) {
  static ScriptToFontMap script_font_map;
  static bool initialized = false;
  if (!initialized) {
    InitializeScriptFontMap(script_font_map);
    initialized = true;
  }

  if (script == USCRIPT_INVALID_CODE)
    return 0;
  DCHECK_LT(script, USCRIPT_CODE_LIMIT);
  if (generic == FontDescription::kMonospaceFamily) {
    const UChar* monospace_family = FindMonospaceFontForScript(script);
    if (monospace_family)
      return monospace_family;
  }
  if (!script_font_map[script].candidate_family_names.empty())
    FindFirstExistingCandidateFont(script_font_map[script], font_manager);
  return script_font_map[script].family_name;
}

// FIXME:
//  - Handle 'Inherited', 'Common' and 'Unknown'
//    (see http://www.unicode.org/reports/tr24/#Usage_Model )
//    For 'Inherited' and 'Common', perhaps we need to
//    accept another parameter indicating the previous family
//    and just return it.
//  - All the characters (or characters up to the point a single
//    font can cover) need to be taken into account
const UChar* GetFallbackFamily(UChar32 character,
                               FontDescription::GenericFamilyType generic,
                               const LayoutLocale* content_locale,
                               UScriptCode* script_checked,
                               FontFallbackPriority fallback_priority,
                               SkFontMgr* font_manager) {
  DCHECK(character);
  DCHECK(font_manager);
  UBlockCode block = fallback_priority == FontFallbackPriority::kEmojiEmoji
                         ? UBLOCK_EMOTICONS
                         : ublock_getCode(character);
  const UChar* family = GetFontBasedOnUnicodeBlock(block, font_manager);
  if (family) {
    if (script_checked)
      *script_checked = USCRIPT_INVALID_CODE;
    return family;
  }

  UScriptCode script = GetScript(character);

  // For the full-width ASCII characters (U+FF00 - U+FF5E), use the font for
  // Han (determined in a locale-dependent way above). Full-width ASCII
  // characters are rather widely used in Japanese and Chinese documents and
  // they're fully covered by Chinese, Japanese and Korean fonts.
  if (0xFF00 < character && character < 0xFF5F)
    script = USCRIPT_HAN;

  if (script == USCRIPT_COMMON)
    script = GetScriptBasedOnUnicodeBlock(character);

  // For unified-Han scripts, try the lang attribute, system, or
  // accept-languages.
  if (script == USCRIPT_HAN) {
    if (const LayoutLocale* locale_for_han =
            LayoutLocale::LocaleForHan(content_locale))
      script = locale_for_han->GetScriptForHan();
    // If still unknown, USCRIPT_HAN uses UI locale.
    // See initializeScriptFontMap().
  }

  family = GetFontFamilyForScript(script, generic, font_manager);
  // Another lame work-around to cover non-BMP characters.
  // If the font family for script is not found or the character is
  // not in BMP (> U+FFFF), we resort to the hard-coded list of
  // fallback fonts for now.
  if (!family || character > 0xFFFF) {
    int plane = character >> 16;
    switch (plane) {
      case 1:
        family = u"code2001";
        break;
      case 2:
        // Use a Traditional Chinese ExtB font if in Traditional Chinese locale.
        // Otherwise, use a Simplified Chinese ExtB font. Windows Japanese
        // fonts do support a small subset of ExtB (that are included in JIS X
        // 0213), but its coverage is rather sparse.
        // Eventually, this should be controlled by lang/xml:lang.
        if (icu::Locale::getDefault() == icu::Locale::getTraditionalChinese())
          family = u"pmingliu-extb";
        else
          family = u"simsun-extb";
        break;
      default:
        family = u"lucida sans unicode";
    }
  }

  if (script_checked)
    *script_checked = script;
  return family;
}

bool GetOutOfProcessFallbackFamily(
    UChar32 character,
    FontDescription::GenericFamilyType generic_family,
    String bcp47_language_tag,
    FontFallbackPriority,
    const mojo::Remote<mojom::blink::DWriteFontProxy>& service,
    String* fallback_family,
    SkFontStyle* fallback_style) {
  String base_family_name_approximation;
  switch (generic_family) {
    case FontDescription::kMonospaceFamily:
      base_family_name_approximation = kCourierNew;
      break;
    case FontDescription::kSansSerifFamily:
      base_family_name_approximation = kArial;
      break;
    default:
      base_family_name_approximation = kTimesNewRoman;
  }

  mojom::blink::FallbackFamilyAndStylePtr fallback_family_and_style;
  bool mojo_result = service->FallbackFamilyAndStyleForCodepoint(
      base_family_name_approximation, bcp47_language_tag, character,
      &fallback_family_and_style);

  SECURITY_DCHECK(fallback_family);
  SECURITY_DCHECK(fallback_style);
  *fallback_family = fallback_family_and_style->fallback_family_name;
  *fallback_style = SkFontStyle(
      fallback_family_and_style->weight, fallback_family_and_style->width,
      static_cast<SkFontStyle::Slant>(fallback_family_and_style->slant));
  return mojo_result;
}

}  // namespace blink
