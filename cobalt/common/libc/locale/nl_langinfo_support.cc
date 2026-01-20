// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License-for-dev at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/common/libc/locale/nl_langinfo_support.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "cobalt/common/libc/locale/lconv_support.h"
#include "starboard/common/log.h"
#include "third_party/icu/source/common/unicode/localebuilder.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/i18n/unicode/dcfmtsym.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"

namespace cobalt {
namespace {

constexpr UChar kSpace = ' ';
constexpr UChar kComma = ',';
constexpr UChar kArabicComma = 0x060C;
constexpr UChar kMyanmarComma = 0x104A;

constexpr std::string_view kFormatChars = "yMdELGHhKkmaszv";
constexpr char kSkeletonDtFmt[] = "yMMMEdjmsz";
constexpr char kSkeletonTFmtAmPm[] = "ahms";

// Convenience method to convert icu::UnicodeStrings to std::string.
std::string ToUtf8(const icu::UnicodeString& us) {
  std::string out;
  us.toUTF8String(out);
  return out;
}

// Helper method to remove redundant and unecesssary spaces in POSIX
// formatting strings that were converted from ICU. Since our converter
// does not support specific ICU symbols (due to no 1 to 1 conversion,
// our implementation does not support the symbol), there are instances where
// there are extra gaps in the converted string.
std::string CollapseSpaces(const std::string& input) {
  std::string result;
  result.reserve(input.length());

  bool lastWasSpace = false;
  for (char c : input) {
    if (c == kSpace) {
      if (!lastWasSpace) {
        result += kSpace;
        lastWasSpace = true;
      }
    } else {
      result += c;
      lastWasSpace = false;
    }
  }

  if (!result.empty() && result.back() == kSpace) {
    result.pop_back();
  }

  return result;
}

// Convenience method to convert a POSIX locale string to the ICU string format.
// Some POSIX strings like sr_RS@latin require special handling to be fully
// translated from POSIX to ICU.
icu::Locale GetCorrectICULocale(const std::string& posix_name) {
  UErrorCode status = U_ZERO_ERROR;

  char canonical_buffer[ULOC_FULLNAME_CAPACITY];
  uloc_canonicalize(posix_name.c_str(), canonical_buffer,
                    ULOC_FULLNAME_CAPACITY, &status);

  if (U_FAILURE(status)) {
    return icu::Locale::createCanonical(posix_name.c_str());
  }

  icu::Locale loc = icu::Locale::createFromName(canonical_buffer);
  const char* variant = loc.getVariant();

  // ICU will store the modifier variable (e.g. @latin) in its variant field, if
  // it exists. For ICU to correctly translate the string, we must convert the
  // script to its shortname and set it in the script field.
  if (variant && *variant) {
    UScriptCode codes[1];

    UErrorCode script_status = U_ZERO_ERROR;
    int32_t num_codes = uscript_getCode(variant, codes, 1, &script_status);

    if (U_SUCCESS(script_status) && num_codes == 1) {
      UScriptCode actual_script = codes[0];

      if (actual_script != USCRIPT_INVALID_CODE &&
          actual_script != USCRIPT_UNKNOWN) {
        const char* short_name = uscript_getShortName(actual_script);

        if (short_name) {
          icu::LocaleBuilder builder;
          builder.setLanguage(loc.getLanguage());
          builder.setRegion(loc.getCountry());
          builder.setScript(short_name);

          return builder.build(status);
        }
      }
    }
  }

  return loc;
}

// Helper function to convert an ICU date/time formatted token to a
// corresponding POSIX token, if such a conversion exists. ICU and POSIX use
// different systems to represent date format strings, and this function will
// make a best attempt to convert an ICU token to its POSIX equivalent.
std::string MapIcuTokenToPosix(const icu::UnicodeString& token) {
  constexpr std::array<std::pair<std::string_view, const char*>, 46>
      kIcuToPosixMap = {
          {// Date
           {"yyyy", "%Y"},
           {"yy", "%y"},
           {"y", "%Y"},
           {"MMMM", "%B"},
           {"MMM", "%b"},
           {"MM", "%m"},
           {"M", "%m"},
           {"dd", "%d"},
           {"d", "%d"},
           {"EEEE", "%A"},
           {"EEE", "%a"},
           {"EE", "%a"},
           {"E", "%a"},
           // Time
           {"HH", "%H"},
           {"H", "%k"},
           {"kk", "%H"},
           {"k", "%H"},
           {"hh", "%I"},
           {"h", "%l"},
           {"K", "%l"},
           {"KK", "%I"},
           {"mm", "%M"},
           {"m", "%M"},
           {"ss", "%S"},
           {"s", "%S"},
           // AM/PM (all variants map to the same POSIX token)
           {"a", "%p"},
           {"aa", "%p"},
           {"aaa", "%p"},
           {"aaaa", "%p"},
           // Era (G) (Currently not supported, will return the empty string if
           // seen)
           {"G", ""},
           {"GG", ""},
           {"GGG", ""},
           {"GGGG", ""},
           {"GGGGG", ""},
           // ICU Timezone specific symbols (POSIX has no direct equivalent for
           // these symbols, so we simply just return "%Z", which is the
           // timezone name/abbreviation)
           {"z", "%Z"},
           {"zz", "%Z"},
           {"zzz", "%Z"},
           {"zzzz", "%Z"},
           {"v", "%Z"},
           {"vv", "%Z"},
           {"vvv", "%Z"},
           {"vvvv", "%Z"},
           {"V", "%Z"},
           {"VV", "%Z"},
           {"VVV", "%Z"},
           {"VVVV", "%Z"}}};

  std::string token_str = ToUtf8(token);
  for (const auto& entry : kIcuToPosixMap) {
    if (entry.first == token_str) {
      return entry.second;
    }
  }

  return token_str;
}

// Given a locale and a format skeleton string, GetPatternFromSkeleton will try
// to generate an ICU pattern from the skeleton string. Since each locale format
// might have small differences between one another, we try to use a general
// skeleton string to construct a pattern. ICU will take this skeleton string
// and try to construct a format pattern following any formatting rules
// expectations a given locale may have.
icu::UnicodeString GetPatternFromSkeleton(const std::string& locale_id,
                                          const icu::UnicodeString& skeleton) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale = GetCorrectICULocale(locale_id);

  // 1. Create the Generator for this locale
  icu::DateTimePatternGenerator* generator =
      icu::DateTimePatternGenerator::createInstance(locale, status);

  if (U_FAILURE(status)) {
    return "";
  }

  icu::UnicodeString pattern = generator->getBestPattern(skeleton, status);

  delete generator;
  return pattern;
}

// Helper to process the current token and append it to the result.
void FlushToken(icu::UnicodeString& currentToken, std::string& result) {
  if (!currentToken.isEmpty()) {
    result += MapIcuTokenToPosix(currentToken);
    currentToken.remove();
  }
}

// IcuPatterToPosix will iterate through an ICU format pattern, translating any
// ICU-specific tokens to a readable POSIX format. It will go through the ICU
// pattern character by character, and will construct the corresponding POSIX
// pattern string.
std::string IcuPatternToPosix(const icu::UnicodeString& pattern) {
  std::string result;
  bool inQuote = false;
  icu::UnicodeString currentToken;

  for (int32_t i = 0; i < pattern.length(); ++i) {
    const UChar c = pattern[i];

    // 1. Handle quotes, which toggle the inQuote state.
    if (c == '\'') {
      if (i + 1 < pattern.length() && pattern[i + 1] == '\'') {
        FlushToken(currentToken, result);  // Flush before adding literal
        result += "'";
        i++;  // Skip the second quote
      } else {
        inQuote = !inQuote;
      }
      // 2. Handle characters inside a quoted section.
    } else if (inQuote) {
      FlushToken(currentToken, result);  // Flush before adding literal
      result += ToUtf8(icu::UnicodeString(c));
      // 3. Handle special ICU format characters.
    } else if (kFormatChars.find(c) != std::string::npos) {
      if (currentToken.isEmpty() || currentToken[0] == c) {
        currentToken.append(c);
      } else {
        FlushToken(currentToken, result);
        currentToken.append(c);
      }
      // 4. Handle all other characters (literals like ':', '/', etc.).
    } else {
      FlushToken(currentToken, result);

      // Filter out commas and certain types of spaces.
      if (c == kComma || c == kMyanmarComma || c == kArabicComma ||
          (c == kSpace && !result.empty() && result.back() == ':')) {
        continue;
      }
      if (c == 0x00A0 || c == 0x202F) {
        result += " ";
      } else {
        if (c < 0x80) {
          result += static_cast<char>(c);
        } else {
          result += ToUtf8(icu::UnicodeString(c));
        }
      }
    }
  }

  // Flush any remaining token at the end of the string.
  FlushToken(currentToken, result);
  return result;
}

// Helper method to retrieve the requested ICU pattern. This function
// accesses the locale's format pattern according to the selected |date_style|
// and |time_style| selected. It then returns this received pattern inside a
// icu::UnicodeString.
icu::UnicodeString GetPatternFromStyle(const std::string& locale_id,
                                       UDateFormatStyle date_style,
                                       UDateFormatStyle time_style) {
  UErrorCode status = U_ZERO_ERROR;

  UDateFormat* fmt = udat_open(time_style, date_style, locale_id.c_str(),
                               nullptr, 0, nullptr, 0, &status);

  if (U_FAILURE(status)) {
    return icu::UnicodeString();
  }

  constexpr int32_t kStackBufferSize = 128;
  UChar stack_buffer[kStackBufferSize];

  int32_t len =
      udat_toPattern(fmt, false, stack_buffer, kStackBufferSize, &status);

  icu::UnicodeString result;

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    // 3. Fallback: If pattern is huge (>128 chars), use heap allocation.
    status = U_ZERO_ERROR;
    std::vector<UChar> heap_buffer(len + 1);
    udat_toPattern(fmt, false, heap_buffer.data(), len + 1, &status);

    if (U_SUCCESS(status)) {
      result = icu::UnicodeString(heap_buffer.data(), len);
    }
  } else if (U_SUCCESS(status)) {
    // 4. Success: Copy directly from stack buffer (Fast)
    result = icu::UnicodeString(stack_buffer, len);
  }

  udat_close(fmt);

  return result;
}
}  // namespace

std::string GetLocalizedDateSymbol(const std::string& locale,
                                   TimeNameType type,
                                   int index) {
  std::string result;

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale = GetCorrectICULocale(locale);

  icu::DateFormatSymbols syms(icu_locale, status);
  if (U_FAILURE(status)) {
    return "";
  }

  int count = 0;

  switch (type) {
    case TimeNameType::kDay:
      result =
          ToUtf8(syms.getWeekdays(count, icu::DateFormatSymbols::STANDALONE,
                                  icu::DateFormatSymbols::WIDE)[index + 1]);
      break;
    case TimeNameType::kAbbrevDay:
      result = ToUtf8(
          syms.getWeekdays(count, icu::DateFormatSymbols::STANDALONE,
                           icu::DateFormatSymbols::ABBREVIATED)[index + 1]);
      break;
    case TimeNameType::kMonth:
      result = ToUtf8(syms.getMonths(count, icu::DateFormatSymbols::STANDALONE,
                                     icu::DateFormatSymbols::WIDE)[index]);
      break;
    case TimeNameType::kAbbrevMonth:
      result =
          ToUtf8(syms.getMonths(count, icu::DateFormatSymbols::STANDALONE,
                                icu::DateFormatSymbols::ABBREVIATED)[index]);
      break;
    case TimeNameType::kAmPm:
      result = ToUtf8(syms.getAmPmStrings(count)[index]);
  }

  return result;
}

std::string NlGetNumericData(const std::string& locale, nl_item type) {
  std::string result;

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale = GetCorrectICULocale(locale);

  icu::DecimalFormatSymbols syms(icu_locale, status);
  if (U_FAILURE(status)) {
    return "";
  }

  switch (type) {
    case RADIXCHAR:
      result = ToUtf8(
          syms.getSymbol(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol));
      break;
    case THOUSEP:
      result = ToUtf8(
          syms.getSymbol(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol));
  }

  return result;
}

std::string GetPosixPattern(const std::string& locale, nl_item item) {
  icu::UnicodeString icu_pattern;

  switch (item) {
    case D_FMT:
      icu_pattern = GetPatternFromStyle(locale, UDAT_SHORT, UDAT_NONE);
      break;
    case T_FMT:
      icu_pattern = GetPatternFromStyle(locale, UDAT_NONE, UDAT_MEDIUM);
      break;
    case D_T_FMT:
      static const icu::UnicodeString kDtPattern(kSkeletonDtFmt);
      icu_pattern = GetPatternFromSkeleton(locale, kDtPattern);
      break;
    case T_FMT_AMPM:
      static const icu::UnicodeString kTAmPmPattern(kSkeletonTFmtAmPm);
      icu_pattern = GetPatternFromSkeleton(locale, kTAmPmPattern);
      break;
    default:
      icu_pattern = "";
  }

  std::string converted_pattern = IcuPatternToPosix(icu_pattern);
  return CollapseSpaces(converted_pattern);
}
}  // namespace cobalt
