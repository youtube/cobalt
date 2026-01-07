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

bool IsPatternChar(UChar c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

std::string IcuPatternToPosix(const icu::UnicodeString& pattern) {
  std::stringstream result;
  bool in_quote = false;

  for (int32_t i = 0; i < pattern.length(); ++i) {
    UChar c = pattern[i];

    // 1. Handle Quotes (Literal Text)
    if (c == '\'') {
      // Check for escaped quote ('')
      if (i + 1 < pattern.length() && pattern[i + 1] == '\'') {
        result << '\'';
        ++i;  // Skip next
      } else {
        in_quote = !in_quote;  // Toggle state
      }
      continue;
    }
    if (in_quote) {
      result << (char)c;  // Copy literal
      continue;
    }

    // 2. Handle Non-Pattern (Separators like / . - space)
    if (!IsPatternChar(c)) {
      result << (char)c;
      continue;
    }

    // 3. Parse Token (Count repetitions: e.g., "yyyy" -> count=4)
    int count = 1;
    while (i + 1 < pattern.length() && pattern[i + 1] == c) {
      count++;
      i++;
    }

    // 4. Map ICU Token to POSIX
    switch (c) {
      case 'y':  // Year
        // POSIX usually treats 'y' (1 count) as full year too.
        // yy = %y (2 digit), yyyy = %Y (4 digit).
        if (count == 2) {
          result << "%y";
        } else {
          result << "%Y";
        }
        break;
      case 'M':  // Month
      case 'L':  // Stand-alone Month (treated same in POSIX fmt)
        if (count >= 4) {
          result << "%B";  // Full name
        } else if (count == 3) {
          result << "%b";  // Abbrev
        } else {
          result << "%m";  // Number
        }
        break;
      case 'd':  // Day
        // POSIX %d usually implies zero-padding (01), %e is space-padding ( 1).
        // ICU 'd' is minimal digits (1), 'dd' is zero-padded (01).
        // Standard D_FMT usually expects %d for both.
        result << "%d";
        break;
      case 'E':  // Day of Week
        if (count >= 4) {
          result << "%A";  // Full
        } else {
          result << "%a";  // Abbrev
        }
        break;
      case 'a':  // AM/PM
        result << "%p";
        break;
      case 'H':  // 0-23 Hour
      case 'k':  // 1-24 Hour
        result << "%H";
        break;
      case 'h':  // 1-12 Hour
      case 'K':  // 0-11 Hour
        result << "%I";
        break;
      case 'm':  // Minute
        result << "%M";
        break;
      case 's':  // Second
        result << "%S";
        break;
      default:
        // Unknown token? Ignore or output raw.
        break;
    }
  }
  return result.str();
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

std::string GetD_FMT(const std::string& locale) {
  icu::UnicodeString icu_pattern =
      GetPatternFromStyle(locale,
                          UDAT_SHORT,  // Date Style
                          UDAT_NONE);  // Time Style

  // 3. Convert
  return IcuPatternToPosix(icu_pattern);
}

}  // namespace cobalt
