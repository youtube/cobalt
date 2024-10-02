// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/layout_locale.h"

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/hyphenation.h"
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/text/locale_to_script_mapping.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

#include <hb.h>
#include <unicode/locid.h>
#include <unicode/ulocdata.h>

namespace blink {

namespace {

struct PerThreadData {
  HashMap<AtomicString,
          scoped_refptr<LayoutLocale>,
          CaseFoldingHashTraits<AtomicString>>
      locale_map;
  const LayoutLocale* default_locale = nullptr;
  const LayoutLocale* system_locale = nullptr;
  const LayoutLocale* default_locale_for_han = nullptr;
  bool default_locale_for_han_computed = false;
  String current_accept_languages;
};

PerThreadData& GetPerThreadData() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<PerThreadData>, data, ());
  return *data;
}

struct DelimiterConfig {
  ULocaleDataDelimiterType type;
  UChar* result;
};
// Use  ICU ulocdata to find quote delimiters for an ICU locale
// https://unicode-org.github.io/icu-docs/apidoc/dev/icu4c/ulocdata_8h.html#a0bf1fdd1a86918871ae2c84b5ce8421f
scoped_refptr<QuotesData> GetQuotesDataForLanguage(const char* locale) {
  UErrorCode status = U_ZERO_ERROR;
  // Expect returned buffer size is 1 to match QuotesData type
  constexpr int ucharDelimMaxLength = 1;

  ULocaleData* uld = ulocdata_open(locale, &status);
  if (U_FAILURE(status)) {
    ulocdata_close(uld);
    return nullptr;
  }
  UChar open1[ucharDelimMaxLength], close1[ucharDelimMaxLength],
      open2[ucharDelimMaxLength], close2[ucharDelimMaxLength];

  int32_t delimResultLength;
  struct DelimiterConfig delimiters[] = {
      {ULOCDATA_QUOTATION_START, open1},
      {ULOCDATA_QUOTATION_END, close1},
      {ULOCDATA_ALT_QUOTATION_START, open2},
      {ULOCDATA_ALT_QUOTATION_END, close2},
  };
  for (DelimiterConfig delim : delimiters) {
    delimResultLength = ulocdata_getDelimiter(uld, delim.type, delim.result,
                                              ucharDelimMaxLength, &status);
    if (U_FAILURE(status) || delimResultLength != 1) {
      ulocdata_close(uld);
      return nullptr;
    }
  }
  ulocdata_close(uld);

  return QuotesData::Create(open1[0], close1[0], open2[0], close2[0]);
}

}  // namespace

static hb_language_t ToHarfbuzLanguage(const AtomicString& locale) {
  std::string locale_as_latin1 = locale.Latin1();
  return hb_language_from_string(locale_as_latin1.data(),
                                 static_cast<int>(locale_as_latin1.length()));
}

// SkFontMgr uses two/three-letter language code with an optional ISO 15924
// four-letter script code, in POSIX style (with '-' as the separator,) such as
// "zh-Hant" and "zh-Hans". See `fonts.xml`.
static const char* ToSkFontMgrLocale(UScriptCode script) {
  switch (script) {
    case USCRIPT_KATAKANA_OR_HIRAGANA:
      return "ja";
    case USCRIPT_HANGUL:
      return "ko";
    case USCRIPT_SIMPLIFIED_HAN:
      return "zh-Hans";
    case USCRIPT_TRADITIONAL_HAN:
      return "zh-Hant";
    default:
      return nullptr;
  }
}

const char* LayoutLocale::LocaleForSkFontMgr() const {
  if (!string_for_sk_font_mgr_.empty())
    return string_for_sk_font_mgr_.c_str();

  if (const char* sk_font_mgr_locale = ToSkFontMgrLocale(script_)) {
    string_for_sk_font_mgr_ = sk_font_mgr_locale;
    DCHECK(!string_for_sk_font_mgr_.empty());
    return string_for_sk_font_mgr_.c_str();
  }

  const icu::Locale locale(Ascii().c_str());
  const char* language = locale.getLanguage();
  string_for_sk_font_mgr_ = language && *language ? language : "und";
  const char* script = locale.getScript();
  if (script && *script)
    string_for_sk_font_mgr_ = string_for_sk_font_mgr_ + "-" + script;
  DCHECK(!string_for_sk_font_mgr_.empty());
  return string_for_sk_font_mgr_.c_str();
}

void LayoutLocale::ComputeScriptForHan() const {
  if (IsUnambiguousHanScript(script_)) {
    script_for_han_ = script_;
    has_script_for_han_ = true;
    return;
  }

  script_for_han_ = ScriptCodeForHanFromSubtags(string_);
  if (script_for_han_ == USCRIPT_COMMON)
    script_for_han_ = USCRIPT_SIMPLIFIED_HAN;
  else
    has_script_for_han_ = true;
  DCHECK(IsUnambiguousHanScript(script_for_han_));
}

UScriptCode LayoutLocale::GetScriptForHan() const {
  if (script_for_han_ == USCRIPT_COMMON)
    ComputeScriptForHan();
  return script_for_han_;
}

bool LayoutLocale::HasScriptForHan() const {
  if (script_for_han_ == USCRIPT_COMMON)
    ComputeScriptForHan();
  return has_script_for_han_;
}

// static
const LayoutLocale* LayoutLocale::LocaleForHan(
    const LayoutLocale* content_locale) {
  if (content_locale && content_locale->HasScriptForHan())
    return content_locale;

  PerThreadData& data = GetPerThreadData();
  if (UNLIKELY(!data.default_locale_for_han_computed)) {
    // Use the first acceptLanguages that can disambiguate.
    Vector<String> languages;
    data.current_accept_languages.Split(',', languages);
    for (String token : languages) {
      token = token.StripWhiteSpace();
      const LayoutLocale* locale = LayoutLocale::Get(AtomicString(token));
      if (locale->HasScriptForHan()) {
        data.default_locale_for_han = locale;
        break;
      }
    }
    if (!data.default_locale_for_han) {
      const LayoutLocale& default_locale = GetDefault();
      if (default_locale.HasScriptForHan())
        data.default_locale_for_han = &default_locale;
    }
    if (!data.default_locale_for_han) {
      const LayoutLocale& system_locale = GetSystem();
      if (system_locale.HasScriptForHan())
        data.default_locale_for_han = &system_locale;
    }
    data.default_locale_for_han_computed = true;
  }
  return data.default_locale_for_han;
}

const char* LayoutLocale::LocaleForHanForSkFontMgr() const {
  const char* locale = ToSkFontMgrLocale(GetScriptForHan());
  DCHECK(locale);
  return locale;
}

void LayoutLocale::ComputeCaseMapLocale() const {
  DCHECK(!case_map_computed_);
  case_map_computed_ = true;
  locale_for_case_map_ = CaseMap::Locale(LocaleString());
}

LayoutLocale::LayoutLocale(const AtomicString& locale)
    : string_(locale),
      harfbuzz_language_(ToHarfbuzLanguage(locale)),
      script_(LocaleToScriptCodeForFontSelection(locale)),
      script_for_han_(USCRIPT_COMMON),
      has_script_for_han_(false),
      hyphenation_computed_(false),
      quotes_data_computed_(false),
      case_map_computed_(false) {}

// static
const LayoutLocale* LayoutLocale::Get(const AtomicString& locale) {
  if (locale.IsNull())
    return nullptr;

  auto result = GetPerThreadData().locale_map.insert(locale, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = base::AdoptRef(new LayoutLocale(locale));
  return result.stored_value->value.get();
}

// static
const LayoutLocale& LayoutLocale::GetDefault() {
  PerThreadData& data = GetPerThreadData();
  if (UNLIKELY(!data.default_locale)) {
    AtomicString language = DefaultLanguage();
    data.default_locale =
        LayoutLocale::Get(!language.empty() ? language : "en");
  }
  return *data.default_locale;
}

// static
const LayoutLocale& LayoutLocale::GetSystem() {
  PerThreadData& data = GetPerThreadData();
  if (UNLIKELY(!data.system_locale)) {
    // Platforms such as Windows can give more information than the default
    // locale, such as "en-JP" for English speakers in Japan.
    String name = icu::Locale::getDefault().getName();
    data.system_locale =
        LayoutLocale::Get(AtomicString(name.Replace('_', '-')));
  }
  return *data.system_locale;
}

scoped_refptr<LayoutLocale> LayoutLocale::CreateForTesting(
    const AtomicString& locale) {
  return base::AdoptRef(new LayoutLocale(locale));
}

Hyphenation* LayoutLocale::GetHyphenation() const {
  if (hyphenation_computed_)
    return hyphenation_.get();

  hyphenation_computed_ = true;
  hyphenation_ = Hyphenation::PlatformGetHyphenation(LocaleString());
  return hyphenation_.get();
}

void LayoutLocale::SetHyphenationForTesting(
    const AtomicString& locale_string,
    scoped_refptr<Hyphenation> hyphenation) {
  const LayoutLocale& locale = ValueOrDefault(Get(locale_string));
  locale.hyphenation_computed_ = true;
  locale.hyphenation_ = std::move(hyphenation);
}

scoped_refptr<QuotesData> LayoutLocale::GetQuotesData() const {
  if (quotes_data_computed_)
    return quotes_data_;
  quotes_data_computed_ = true;

  // BCP 47 uses '-' as the delimiter but ICU uses '_'.
  // https://tools.ietf.org/html/bcp47
  String normalized_lang = LocaleString();
  normalized_lang.Replace('-', '_');

  UErrorCode status = U_ZERO_ERROR;
  // Use uloc_openAvailableByType() to find all CLDR recognized locales
  // https://unicode-org.github.io/icu-docs/apidoc/dev/icu4c/uloc_8h.html#aa0332857185774f3e0520a0823c14d16
  UEnumeration* ulocales =
      uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
  if (U_FAILURE(status)) {
    uenum_close(ulocales);
    return nullptr;
  }

  // Try to find exact match
  while (const char* loc = uenum_next(ulocales, nullptr, &status)) {
    if (U_FAILURE(status)) {
      uenum_close(ulocales);
      return nullptr;
    }
    if (EqualIgnoringASCIICase(loc, normalized_lang)) {
      quotes_data_ = GetQuotesDataForLanguage(loc);
      uenum_close(ulocales);
      return quotes_data_;
    }
  }
  uenum_close(ulocales);

  // No exact match, try to find without subtags.
  wtf_size_t hyphen_offset = normalized_lang.ReverseFind('_');
  if (hyphen_offset == kNotFound)
    return nullptr;
  normalized_lang = normalized_lang.Substring(0, hyphen_offset);
  ulocales = uloc_openAvailableByType(ULOC_AVAILABLE_DEFAULT, &status);
  if (U_FAILURE(status)) {
    uenum_close(ulocales);
    return nullptr;
  }
  while (const char* loc = uenum_next(ulocales, nullptr, &status)) {
    if (U_FAILURE(status)) {
      uenum_close(ulocales);
      return nullptr;
    }
    if (EqualIgnoringASCIICase(loc, normalized_lang)) {
      quotes_data_ = GetQuotesDataForLanguage(loc);
      uenum_close(ulocales);
      return quotes_data_;
    }
  }
  uenum_close(ulocales);
  return nullptr;
}

AtomicString LayoutLocale::LocaleWithBreakKeyword(
    LineBreakIteratorMode mode) const {
  if (string_.empty())
    return string_;

  // uloc_setKeywordValue_58 has a problem to handle "@" in the original
  // string. crbug.com/697859
  if (string_.Contains('@'))
    return string_;

  std::string utf8_locale = string_.Utf8();
  Vector<char> buffer(static_cast<wtf_size_t>(utf8_locale.length() + 11), 0);
  memcpy(buffer.data(), utf8_locale.c_str(), utf8_locale.length());

  const char* keyword_value = nullptr;
  switch (mode) {
    default:
      NOTREACHED();
      [[fallthrough]];
    case LineBreakIteratorMode::kDefault:
      // nullptr will cause any existing values to be removed.
      break;
    case LineBreakIteratorMode::kNormal:
      keyword_value = "normal";
      break;
    case LineBreakIteratorMode::kStrict:
      keyword_value = "strict";
      break;
    case LineBreakIteratorMode::kLoose:
      keyword_value = "loose";
      break;
  }

  ICUError status;
  int32_t length_needed = uloc_setKeywordValue(
      "lb", keyword_value, buffer.data(), buffer.size(), &status);
  if (U_SUCCESS(status))
    return AtomicString::FromUTF8(buffer.data(), length_needed);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    buffer.Grow(length_needed + 1);
    memset(buffer.data() + utf8_locale.length(), 0,
           buffer.size() - utf8_locale.length());
    status = U_ZERO_ERROR;
    int32_t length_needed2 = uloc_setKeywordValue(
        "lb", keyword_value, buffer.data(), buffer.size(), &status);
    DCHECK_EQ(length_needed, length_needed2);
    if (U_SUCCESS(status) && length_needed == length_needed2)
      return AtomicString::FromUTF8(buffer.data(), length_needed);
  }

  NOTREACHED();
  return string_;
}

// static
void LayoutLocale::AcceptLanguagesChanged(const String& accept_languages) {
  PerThreadData& data = GetPerThreadData();
  if (data.current_accept_languages == accept_languages)
    return;

  data.current_accept_languages = accept_languages;
  data.default_locale_for_han = nullptr;
  data.default_locale_for_han_computed = false;
}

// static
void LayoutLocale::ClearForTesting() {
  GetPerThreadData() = PerThreadData();
}

}  // namespace blink
