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
#include <sstream>
#include <string>

#include "cobalt/common/libc/locale/lconv_support.h"
#include "starboard/common/log.h"
#include "third_party/icu/source/common/unicode/localebuilder.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/i18n/unicode/dcfmtsym.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"

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
  // --- DATE (Existing) ---
  if (token == "yyyy") {
    return "%Y";
  }
  if (token == "yy") {
    return "%y";
  }
  if (token == "y") {
    return "%Y";
  }
  if (token == "MMMM") {
    return "%B";
  }
  if (token == "MMM") {
    return "%b";
  }
  if (token == "MM") {
    return "%m";
  }
  if (token == "M") {
    return "%m";
  }
  if (token == "dd") {
    return "%d";
  }
  if (token == "d") {
    return "%d";
  }
  if (token == "EEEE") {
    return "%A";
  }
  if (token == "EEE") {
    return "%a";
  }
  if (token == "EE") {
    return "%a";
  }
  if (token == "E") {
    return "%a";
  }
  if (token.startsWith("G")) {
    return "";  // Strip Era
  }

  // --- TIME (New) ---

  // Hours
  if (token == "HH") {
    return "%H";  // 00-23
  }
  if (token == "H") {
    return "%k";  // 0-23 (GNU) or "%H" (POSIX)
  }
  if (token == "kk") {
    return "%H";  // 01-24 (Map to %H usually)
  }
  if (token == "k") {
    return "%H";
  }
  if (token == "hh") {
    return "%I";  // 01-12
  }
  if (token == "h") {
    return "%l";  // 1-12 (GNU) or "%I" (POSIX)
  }
  if (token == "K") {
    return "%l";  // 0-11
  }
  if (token == "KK") {
    return "%I";
  }

  // Minutes
  if (token == "mm") {
    return "%M";  // 00-59
  }
  if (token == "m") {
    return "%M";
  }

  // Seconds
  if (token == "ss") {
    return "%S";  // 00-59
  }
  if (token == "s") {
    return "%S";
  }

  // AM/PM
  if (token == "a" || token == "aa" || token == "aaa" || token == "aaaa") {
    return "%p";
  }

  // Timezone (Generic)
  // POSIX usually handles %Z or %z. ICU 'z', 'v', 'V' map loosely here.
  if (token.startsWith("z") || token.startsWith("v") || token.startsWith("V")) {
    return "%Z";
  }

  // Default
  std::string s;
  token.toUTF8String(s);
  return s;
}

std::string IcuPatternToPosix(const icu::UnicodeString& pattern) {
  std::string result;
  bool inQuote = false;
  icu::UnicodeString currentToken;

  for (int32_t i = 0; i < pattern.length(); ++i) {
    UChar c = pattern[i];

    // 1. Handle Quotes (The logic that fixes bg_BG)
    if (c == '\'') {
      // Check for escaped quote (two single quotes in a row: '')
      if (i + 1 < pattern.length() && pattern[i + 1] == '\'') {
        result += "'";  // It's just a literal single quote
        i++;            // Skip the second quote
      } else {
        // Toggle quote mode
        inQuote = !inQuote;
      }
      continue;
    }

    // 2. Handle Quoted Literals (Copy exactly)
    if (inQuote) {
      // If we have pending format tokens, flush them first
      if (!currentToken.isEmpty()) {
        result += MapIcuTokenToPosix(currentToken);
        currentToken.remove();
      }

      // Append the literal char (convert to UTF-8)
      std::string temp;
      icu::UnicodeString(c).toUTF8String(temp);
      result += temp;
      continue;
    }

    // 3. Handle Format Characters
    // (y, M, d, E, etc.)
    bool isFormatChar =
        (c == 'y' || c == 'M' || c == 'd' || c == 'E' || c == 'L' || c == 'G' ||
         // NEW TIME CHARS:
         c == 'H' || c == 'h' || c == 'k' || c == 'K' || c == 'm' || c == 's' ||
         c == 'a' || c == 'z' || c == 'v');

    if (isFormatChar) {
      // If this char matches the current token (e.g. "y" -> "yy"), append it
      if (currentToken.isEmpty() || currentToken[0] == c) {
        currentToken.append(c);
      } else {
        // Different char started (e.g. "MM" -> "d"), flush old token
        result += MapIcuTokenToPosix(currentToken);
        currentToken.remove();
        currentToken.append(c);
      }
    } else {
      // It's a separator (., /, space), flush pending token
      if (!currentToken.isEmpty()) {
        result += MapIcuTokenToPosix(currentToken);
        currentToken.remove();
      }
      // Append the separator literal
      std::string temp;
      icu::UnicodeString(c).toUTF8String(temp);
      result += temp;
    }
  }

  // Flush any remaining token
  if (!currentToken.isEmpty()) {
    result += MapIcuTokenToPosix(currentToken);
  }

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

std::string GetPosixFormat(const std::string& locale, nl_item item) {
  icu::UnicodeString icu_pattern;

  switch (item) {
    case D_FMT:
      icu_pattern = GetPatternFromStyle(locale, UDAT_SHORT, UDAT_NONE);
      break;
    case T_FMT:
      icu_pattern = GetPatternFromStyle(locale, UDAT_NONE, UDAT_MEDIUM);
      break;
    default:
      icu_pattern = "";
  }

  return IcuPatternToPosix(icu_pattern);
}
}  // namespace cobalt
