// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_UTIL_H_
#define NET_FTP_FTP_UTIL_H_
#pragma once

#include <string>

#include "base/string16.h"

namespace base {
class Time;
}

namespace net {

class FtpUtil {
 public:
  // Converts Unix file path to VMS path (must be a file, and not a directory).
  static std::string UnixFilePathToVMS(const std::string& unix_path);

  // Converts Unix directory path to VMS path (must be a directory).
  static std::string UnixDirectoryPathToVMS(const std::string& unix_path);

  // Converts VMS path to Unix-style path.
  static std::string VMSPathToUnix(const std::string& vms_path);

  // Converts abbreviated month (like Nov) to its number (in range 1-12).
  // Note: in some locales abbreviations are more than three letters long,
  // and this function also handles them correctly.
  static bool AbbreviatedMonthToNumber(const string16& text, int* number);

  // Converts a "ls -l" date listing to time. The listing comes in three
  // columns. The first one contains month, the second one contains day
  // of month. The third one is either a time (and then we guess the year based
  // on |current_time|), or is a year (and then we don't know the time).
  static bool LsDateListingToTime(const string16& month,
                                  const string16& day,
                                  const string16& rest,
                                  const base::Time& current_time,
                                  base::Time* result);

  // Skips |columns| columns from |text| (whitespace-delimited), and returns the
  // remaining part, without leading/trailing whitespace.
  static string16 GetStringPartAfterColumns(const string16& text, int columns);
};

}  // namespace net

#endif  // NET_FTP_FTP_UTIL_H_
