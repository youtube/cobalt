// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_UTIL_H_
#define NET_FTP_FTP_UTIL_H_

#include <string>

#include "base/string16.h"

namespace net {

class FtpUtil {
 public:
  // Convert Unix file path to VMS path (must be a file, and not a directory).
  static std::string UnixFilePathToVMS(const std::string& unix_path);

  // Convert Unix directory path to VMS path (must be a directory).
  static std::string UnixDirectoryPathToVMS(const std::string& unix_path);

  // Convert VMS path to Unix-style path.
  static std::string VMSPathToUnix(const std::string& vms_path);

  // Convert three-letter month abbreviation (like Nov) to its number (in range
  // 1-12).
  static bool ThreeLetterMonthToNumber(const string16& text, int* number);
};

}  // namespace net

#endif  // NET_FTP_FTP_UTIL_H_
