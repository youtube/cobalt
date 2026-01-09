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

std::string IcuPatternToPosix(const icu::UnicodeString& pattern) {
  std::string result;
  bool inQuote = false;
  icu::UnicodeString currentToken;

  for (int32_t i = 0; i < pattern.length(); ++i) {
    UChar c = pattern[i];

    // 1. Handle Quotes
    if (c == '\'') {
      if (i + 1 < pattern.length() && pattern[i + 1] == '\'') {
        result += "'";
        i++;
      } else {
        inQuote = !inQuote;
      }
      continue;
    }

    // 2. Handle Quoted Literals
    if (inQuote) {
      if (!currentToken.isEmpty()) {
        result += MapIcuTokenToPosix(currentToken);
        currentToken.remove();
      }
      std::string temp;
      icu::UnicodeString(c).toUTF8String(temp);
      result += temp;
      continue;
    }

    // 3. Handle Format Characters
    bool isFormatChar =
        (c == 'y' || c == 'M' || c == 'd' || c == 'E' || c == 'L' || c == 'G' ||
         c == 'H' || c == 'h' || c == 'k' || c == 'K' || c == 'm' || c == 's' ||
         c == 'a' || c == 'z' || c == 'v');

    if (isFormatChar) {
      if (currentToken.isEmpty() || currentToken[0] == c) {
        currentToken.append(c);
      } else {
        result += MapIcuTokenToPosix(currentToken);
        currentToken.remove();
        currentToken.append(c);
      }
    } else {
      // --- FIXED LOGIC STARTS HERE ---

      // STEP 1: Flush the token BEFORE doing anything else!
      // This ensures "ss" becomes "%S" before we handle the space after it.
      if (!currentToken.isEmpty()) {
        result += MapIcuTokenToPosix(currentToken);
        currentToken.remove();
      }

      // STEP 2: Filter Commas
      if (c == ',' || c == 0x104A || c == 0x060C) {
        continue;
      }

      // STEP 3: Filter "Space after Colon" (The fil_PH specific fix)
      // If we just appended a colon, and now we see a space, skip it.
      // This prevents "12: 30".
      if (c == ' ' && !result.empty() && result.back() == ':') {
        continue;
      }

      // STEP 4: Normalize "Weird" Spaces
      // Now that %S is safely in the string, we can append the space *after*
      // it.
      if (c == 0x00A0 || c == 0x202F) {
        result += " ";
        continue;
      }

      // STEP 5: Standard Append (for . / : - etc)
      std::string temp;
      icu::UnicodeString(c).toUTF8String(temp);
      result += temp;
    }
  }

  // Flush any remaining token at end of string
  if (!currentToken.isEmpty()) {
    result += MapIcuTokenToPosix(currentToken);
  }

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

std::string GetPosixFormat(const std::string& locale, nl_item item) {
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

  return CollapseSpaces(IcuPatternToPosix(icu_pattern));
}
}  // namespace cobalt
