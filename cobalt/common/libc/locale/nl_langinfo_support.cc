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
#include <sstream>
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

// Convenience method to convert icu::UnicodeStrings to std::string.
std::string ToUtf8(const icu::UnicodeString& us) {
  std::string out;
  us.toUTF8String(out);
  return out;
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

std::string CollapseSpaces(const std::string& input) {
  std::string result;
  result.reserve(input.length());  // Optimization

  bool lastWasSpace = false;
  for (char c : input) {
    if (c == ' ') {
      if (!lastWasSpace) {
        result += ' ';
        lastWasSpace = true;
      }
    } else {
      result += c;
      lastWasSpace = false;
    }
  }

  // Trim trailing space if any
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }

  return result;
}

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

  // 2. Ask it to build the best pattern for your ingredients (skeleton)
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

std::string IcuPatternToPosix(const icu::UnicodeString& pattern) {
  std::string result;
  bool inQuote = false;
  icu::UnicodeString currentToken;
  const std::string kFormatChars = "yMdELGHhKkmaszv";

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
      if (c == ',' || c == 0x104A || c == 0x060C ||
          (c == ' ' && !result.empty() && result.back() == ':')) {
        continue;
      }
      if (c == 0x00A0 || c == 0x202F) {
        result += " ";
      } else {
        result += ToUtf8(icu::UnicodeString(c));
      }
    }
  }

  // Flush any remaining token at the end of the string.
  FlushToken(currentToken, result);

  // Final Cleanup: Trim trailing space
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }

  return result;
}

icu::UnicodeString GetPatternFromStyle(const std::string& locale_id,
                                       UDateFormatStyle date_style,
                                       UDateFormatStyle time_style) {
  UErrorCode status = U_ZERO_ERROR;

  // 1. Open C-API Formatter
  UDateFormat* fmt = udat_open(time_style, date_style, locale_id.c_str(),
                               nullptr, 0, nullptr, 0, &status);

  if (U_FAILURE(status)) {
    return icu::UnicodeString();
  }

  // 2. Pre-flight: Get length needed
  // 'false' means we want canonical pattern chars (y, M, d), not localized
  // ones.
  int32_t len = udat_toPattern(fmt, false, nullptr, 0, &status);

  status = U_ZERO_ERROR;  // Clear the expected buffer overflow warning

  // 3. Allocate Buffer
  std::vector<UChar> buffer(len + 1);

  // 4. Fetch the Pattern
  udat_toPattern(fmt, false, buffer.data(), len + 1, &status);

  // 5. Cleanup
  udat_close(fmt);

  if (U_FAILURE(status)) {
    return icu::UnicodeString();
  }

  // 6. Return UnicodeString directly
  return icu::UnicodeString(buffer.data(), len);
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
      icu_pattern = GetPatternFromSkeleton(locale, "yMMMEdjmsz");
      break;
    case T_FMT_AMPM:
      icu_pattern = GetPatternFromSkeleton(locale, "ahms");
      break;
    default:
      icu_pattern = "";
  }

  std::string converted_pattern = IcuPatternToPosix(icu_pattern);
  return CollapseSpaces(converted_pattern);
}
}  // namespace cobalt
