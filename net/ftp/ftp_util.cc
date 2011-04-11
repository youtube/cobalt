// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include <vector>

#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "unicode/datefmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/uchar.h"

// For examples of Unix<->VMS path conversions, see the unit test file. On VMS
// a path looks differently depending on whether it's a file or directory.

namespace net {

// static
std::string FtpUtil::UnixFilePathToVMS(const std::string& unix_path) {
  if (unix_path.empty())
    return std::string();

  StringTokenizer tokenizer(unix_path, "/");
  std::vector<std::string> tokens;
  while (tokenizer.GetNext())
    tokens.push_back(tokenizer.token());

  if (unix_path[0] == '/') {
    // It's an absolute path.

    if (tokens.empty()) {
      DCHECK_EQ(1U, unix_path.length());
      return "[]";
    }

    if (tokens.size() == 1)
      return unix_path.substr(1);  // Drop the leading slash.

    std::string result(tokens[0] + ":[");
    if (tokens.size() == 2) {
      // Don't ask why, it just works that way on VMS.
      result.append("000000");
    } else {
      result.append(tokens[1]);
      for (size_t i = 2; i < tokens.size() - 1; i++)
        result.append("." + tokens[i]);
    }
    result.append("]" + tokens[tokens.size() - 1]);
    return result;
  }

  if (tokens.size() == 1)
    return unix_path;

  std::string result("[");
  for (size_t i = 0; i < tokens.size() - 1; i++)
    result.append("." + tokens[i]);
  result.append("]" + tokens[tokens.size() - 1]);
  return result;
}

// static
std::string FtpUtil::UnixDirectoryPathToVMS(const std::string& unix_path) {
  if (unix_path.empty())
    return std::string();

  std::string path(unix_path);

  if (path[path.length() - 1] != '/')
    path.append("/");

  // Reuse logic from UnixFilePathToVMS by appending a fake file name to the
  // real path and removing it after conversion.
  path.append("x");
  path = UnixFilePathToVMS(path);
  return path.substr(0, path.length() - 1);
}

// static
std::string FtpUtil::VMSPathToUnix(const std::string& vms_path) {
  if (vms_path.empty())
    return ".";

  if (vms_path == "[]")
    return "/";

  std::string result(vms_path);
  if (vms_path[0] == '[') {
    // It's a relative path.
    ReplaceFirstSubstringAfterOffset(&result, 0, "[.", "");
  } else {
    // It's an absolute path.
    result.insert(0, "/");
    ReplaceSubstringsAfterOffset(&result, 0, ":[000000]", "/");
    ReplaceSubstringsAfterOffset(&result, 0, ":[", "/");
  }
  std::replace(result.begin(), result.end(), '.', '/');
  std::replace(result.begin(), result.end(), ']', '/');

  // Make sure the result doesn't end with a slash.
  if (result.length() && result[result.length() - 1] == '/')
    result = result.substr(0, result.length() - 1);

  return result;
}

// static
bool FtpUtil::AbbreviatedMonthToNumber(const string16& text, int* number) {
  icu::UnicodeString unicode_text(text.data(), text.size());

  int32_t locales_count;
  const icu::Locale* locales =
      icu::DateFormat::getAvailableLocales(locales_count);

  // Some FTP servers localize the date listings. To guess the locale,
  // we loop over all available ones.
  for (int32_t locale = 0; locale < locales_count; locale++) {
    UErrorCode status(U_ZERO_ERROR);

    icu::DateFormatSymbols format_symbols(locales[locale], status);

    // If we cannot get format symbols for some locale, it's not a fatal error.
    // Just try another one.
    if (U_FAILURE(status))
      continue;

    int32_t months_count;
    const icu::UnicodeString* months =
        format_symbols.getShortMonths(months_count);

    // Loop over all abbreviated month names in given locale.
    // An alternative solution (to parse |text| in given locale) is more
    // lenient, and may accept more than we want even with setLenient(false).
    for (int32_t month = 0; month < months_count; month++) {
      // Compare (case-insensitive), but just first three characters. Sometimes
      // ICU returns longer strings (for example for Russian locale), and in FTP
      // listings they are abbreviated to just three characters.
      // Note: ICU may also return strings shorter than three characters,
      // and those also should be accepted.
      if (months[month].caseCompare(0, 3, unicode_text, 0) == 0) {
        *number = month + 1;
        return true;
      }
    }
  }

  return false;
}

// static
bool FtpUtil::LsDateListingToTime(const string16& month, const string16& day,
                                  const string16& rest,
                                  const base::Time& current_time,
                                  base::Time* result) {
  base::Time::Exploded time_exploded = { 0 };

  if (!AbbreviatedMonthToNumber(month, &time_exploded.month))
    return false;

  if (!base::StringToInt(day, &time_exploded.day_of_month))
    return false;
  if (time_exploded.day_of_month > 31)
    return false;

  if (!base::StringToInt(rest, &time_exploded.year)) {
    // Maybe it's time. Does it look like time (HH:MM)?
    if (rest.length() == 5 && rest[2] == ':') {
      if (!base::StringToInt(rest.begin(),
                             rest.begin() + 2,
                             &time_exploded.hour))
        return false;

      if (!base::StringToInt(rest.begin() + 3,
                             rest.begin() + 5,
                             &time_exploded.minute))
        return false;
    } else if (rest.length() == 4 && rest[1] == ':') {
      // Sometimes it's just H:MM.
      if (!base::StringToInt(rest.begin(),
                             rest.begin() + 1,
                             &time_exploded.hour))
        return false;

      if (!base::StringToInt(rest.begin() + 2,
                             rest.begin() + 4,
                             &time_exploded.minute))
        return false;
    } else {
      return false;
    }

    // Guess the year.
    base::Time::Exploded current_exploded;
    current_time.LocalExplode(&current_exploded);

    // If it's not possible for the parsed date to be in the current year,
    // use the previous year.
    if (time_exploded.month > current_exploded.month ||
        (time_exploded.month == current_exploded.month &&
         time_exploded.day_of_month > current_exploded.day_of_month)) {
      time_exploded.year = current_exploded.year - 1;
    } else {
      time_exploded.year = current_exploded.year;
    }
  }

  // We don't know the time zone of the listing, so just use local time.
  *result = base::Time::FromLocalExploded(time_exploded);
  return true;
}

// static
string16 FtpUtil::GetStringPartAfterColumns(const string16& text, int columns) {
  base::i18n::UTF16CharIterator iter(&text);

  // TODO(jshin): Is u_isspace the right function to use here?
  for (int i = 0; i < columns; i++) {
    // Skip the leading whitespace.
    while (!iter.end() && u_isspace(iter.get()))
      iter.Advance();

    // Skip the actual text of i-th column.
    while (!iter.end() && !u_isspace(iter.get()))
      iter.Advance();
  }

  string16 result(text.substr(iter.array_pos()));
  TrimWhitespace(result, TRIM_ALL, &result);
  return result;
}

}  // namespace
