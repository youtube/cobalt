// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include <vector>

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"

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
  if (result[result.length() - 1] == '/')
    result = result.substr(0, result.length() - 1);

  return result;
}

// static
bool FtpUtil::ThreeLetterMonthToNumber(const string16& text, int* number) {
  const static char* months[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                  "jul", "aug", "sep", "oct", "nov", "dec" };

  for (size_t i = 0; i < arraysize(months); i++) {
    if (LowerCaseEqualsASCII(text, months[i])) {
      *number = i + 1;
      return true;
    }
  }

  // Special cases for directory listings in German (other three-letter month
  // abbreviations are the same as in English). Note that we don't need to do
  // a case-insensitive compare here. Only "ls -l" style listings may use
  // localized month names, and they will always start capitalized. Also,
  // converting non-ASCII characters to lowercase would be more complicated.
  if (text == UTF8ToUTF16("M\xc3\xa4r")) {
    // The full month name is M-(a-umlaut)-rz (March), which is M-(a-umlaut)r
    // when abbreviated.
    *number = 3;
    return true;
  }
  if (text == ASCIIToUTF16("Mai")) {
    *number = 5;
    return true;
  }
  if (text == ASCIIToUTF16("Okt")) {
    *number = 10;
    return true;
  }
  if (text == ASCIIToUTF16("Dez")) {
    *number = 12;
    return true;
  }

  return false;
}

// static
bool FtpUtil::LsDateListingToTime(const string16& month, const string16& day,
                                  const string16& rest, base::Time* time) {
  base::Time::Exploded time_exploded = { 0 };

  if (!ThreeLetterMonthToNumber(month, &time_exploded.month))
    return false;

  if (!StringToInt(day, &time_exploded.day_of_month))
    return false;

  if (!StringToInt(rest, &time_exploded.year)) {
    // Maybe it's time. Does it look like time (MM:HH)?
    if (rest.length() != 5 || rest[2] != ':')
      return false;

    if (!StringToInt(rest.substr(0, 2), &time_exploded.hour))
      return false;

    if (!StringToInt(rest.substr(3, 2), &time_exploded.minute))
      return false;

    // Use current year.
    base::Time::Exploded now_exploded;
    base::Time::Now().LocalExplode(&now_exploded);
    time_exploded.year = now_exploded.year;
  }

  // We don't know the time zone of the listing, so just use local time.
  *time = base::Time::FromLocalExploded(time_exploded);
  return true;
}

// static
string16 FtpUtil::GetStringPartAfterColumns(const string16& text, int columns) {
  size_t pos = 0;

  for (int i = 0; i < columns; i++) {
    // Skip the leading whitespace.
    while (pos < text.length() && isspace(text[pos]))
      pos++;

    // Skip the actual text of i-th column.
    while (pos < text.length() && !isspace(text[pos]))
      pos++;
  }

  string16 result(text.substr(pos));
  TrimWhitespace(result, TRIM_ALL, &result);
  return result;
}

}  // namespace
