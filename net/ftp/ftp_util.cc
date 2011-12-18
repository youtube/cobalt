// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include <map>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "unicode/datefmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/uchar.h"

using base::StringPiece16;

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

namespace {

// Lazy-initialized map of abbreviated month names.
class AbbreviatedMonthsMap {
 public:
  static AbbreviatedMonthsMap* GetInstance() {
    return Singleton<AbbreviatedMonthsMap>::get();
  }

  // Converts abbreviated month name |text| to its number (in range 1-12).
  // On success returns true and puts the number in |number|.
  bool GetMonthNumber(const string16& text, int* number) {
    // Ignore the case of the month names. The simplest way to handle that
    // is to make everything lowercase.
    string16 text_lower(base::i18n::ToLower(text));

    if (map_.find(text_lower) == map_.end())
      return false;

    *number = map_[text_lower];
    return true;
  }

 private:
  friend struct DefaultSingletonTraits<AbbreviatedMonthsMap>;

  // Constructor, initializes the map based on ICU data. It is much faster
  // to do that just once.
  AbbreviatedMonthsMap() {
    int32_t locales_count;
    const icu::Locale* locales =
        icu::DateFormat::getAvailableLocales(locales_count);

    for (int32_t locale = 0; locale < locales_count; locale++) {
      UErrorCode status(U_ZERO_ERROR);

      icu::DateFormatSymbols format_symbols(locales[locale], status);

      // If we cannot get format symbols for some locale, it's not a fatal
      // error. Just try another one.
      if (U_FAILURE(status))
        continue;

      int32_t months_count;
      const icu::UnicodeString* months =
          format_symbols.getShortMonths(months_count);

      for (int32_t month = 0; month < months_count; month++) {
        string16 month_name(months[month].getBuffer(),
                            static_cast<size_t>(months[month].length()));

        // Ignore the case of the month names. The simplest way to handle that
        // is to make everything lowercase.
        month_name = base::i18n::ToLower(month_name);

        map_[month_name] = month + 1;

        // Sometimes ICU returns longer strings, but in FTP listings a shorter
        // abbreviation is used (for example for the Russian locale). Make sure
        // we always have a map entry for a three-letter abbreviation.
        map_[month_name.substr(0, 3)] = month + 1;
      }
    }
  }

  // Maps lowercase month names to numbers in range 1-12.
  std::map<string16, int> map_;

  DISALLOW_COPY_AND_ASSIGN(AbbreviatedMonthsMap);
};

}  // namespace

// static
bool FtpUtil::AbbreviatedMonthToNumber(const string16& text, int* number) {
  return AbbreviatedMonthsMap::GetInstance()->GetMonthNumber(text, number);
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
      if (!base::StringToInt(StringPiece16(rest.begin(), rest.begin() + 2),
                             &time_exploded.hour)) {
        return false;
      }

      if (!base::StringToInt(StringPiece16(rest.begin() + 3, rest.begin() + 5),
                             &time_exploded.minute)) {
        return false;
      }
    } else if (rest.length() == 4 && rest[1] == ':') {
      // Sometimes it's just H:MM.
      if (!base::StringToInt(StringPiece16(rest.begin(), rest.begin() + 1),
                             &time_exploded.hour)) {
        return false;
      }

      if (!base::StringToInt(StringPiece16(rest.begin() + 2, rest.begin() + 4),
                             &time_exploded.minute)) {
        return false;
      }
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
bool FtpUtil::WindowsDateListingToTime(const string16& date,
                                       const string16& time,
                                       base::Time* result) {
  base::Time::Exploded time_exploded = { 0 };

  // Date should be in format MM-DD-YY[YY].
  std::vector<string16> date_parts;
  base::SplitString(date, '-', &date_parts);
  if (date_parts.size() != 3)
    return false;
  if (!base::StringToInt(date_parts[0], &time_exploded.month))
    return false;
  if (!base::StringToInt(date_parts[1], &time_exploded.day_of_month))
    return false;
  if (!base::StringToInt(date_parts[2], &time_exploded.year))
    return false;
  if (time_exploded.year < 0)
    return false;
  // If year has only two digits then assume that 00-79 is 2000-2079,
  // and 80-99 is 1980-1999.
  if (time_exploded.year < 80)
    time_exploded.year += 2000;
  else if (time_exploded.year < 100)
    time_exploded.year += 1900;

  // Time should be in format HH:MM[(AM|PM)]
  if (time.length() < 5)
    return false;

  std::vector<string16> time_parts;
  base::SplitString(time.substr(0, 5), ':', &time_parts);
  if (time_parts.size() != 2)
    return false;
  if (!base::StringToInt(time_parts[0], &time_exploded.hour))
    return false;
  if (!base::StringToInt(time_parts[1], &time_exploded.minute))
    return false;
  if (!time_exploded.HasValidValues())
    return false;

  if (time.length() > 5) {
    if (time.length() != 7)
      return false;
    string16 am_or_pm(time.substr(5, 2));
    if (EqualsASCII(am_or_pm, "PM")) {
      if (time_exploded.hour < 12)
        time_exploded.hour += 12;
    } else if (EqualsASCII(am_or_pm, "AM")) {
      if (time_exploded.hour == 12)
        time_exploded.hour = 0;
    } else {
      return false;
    }
  }

  // We don't know the time zone of the server, so just use local time.
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
