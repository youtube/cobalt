// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/threading/thread_restrictions.h"

namespace base {

#if defined(GOOGLE_CHROME_BUILD)
static const char kLinuxStandardBaseVersionKey[] = "GOOGLE_RELEASE";
#else
static const char kLinuxStandardBaseVersionKey[] = "DISTRIB_RELEASE";
#endif

const char kLinuxStandardBaseReleaseFile[] = "/etc/lsb-release";

// static
void SysInfo::OperatingSystemVersionNumbers(int32 *major_version,
                                            int32 *minor_version,
                                            int32 *bugfix_version) {
  // The other implementations of SysInfo don't block on the disk.
  // See http://code.google.com/p/chromium/issues/detail?id=60394
  // Perhaps the caller ought to cache this?
  // Temporary allowing while we work the bug out.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // TODO(cmasone): If this gets called a lot, it may kill performance.
  // consider using static variables to cache these values?
  FilePath path(kLinuxStandardBaseReleaseFile);
  std::string contents;
  if (file_util::ReadFileToString(path, &contents)) {
    ParseLsbRelease(contents, major_version, minor_version, bugfix_version);
  }
}

// static
std::string SysInfo::GetLinuxStandardBaseVersionKey() {
  return std::string(kLinuxStandardBaseVersionKey);
}

// static
void SysInfo::ParseLsbRelease(const std::string& lsb_release,
                              int32 *major_version,
                              int32 *minor_version,
                              int32 *bugfix_version) {
  size_t version_key_index = lsb_release.find(kLinuxStandardBaseVersionKey);
  if (std::string::npos == version_key_index) {
    return;
  }
  size_t start_index = lsb_release.find_first_of('=', version_key_index);
  start_index++;  // Move past '='.
  size_t length = lsb_release.find_first_of('\n', start_index) - start_index;
  std::string version = lsb_release.substr(start_index, length);
  StringTokenizer tokenizer(version, ".");
  for (int i = 0; i < 3 && tokenizer.GetNext(); i++) {
    if (0 == i) {
      StringToInt(tokenizer.token_begin(),
                  tokenizer.token_end(),
                  major_version);
      *minor_version = *bugfix_version = 0;
    } else if (1 == i) {
      StringToInt(tokenizer.token_begin(),
                  tokenizer.token_end(),
                  minor_version);
    } else {  // 2 == i
      StringToInt(tokenizer.token_begin(),
                  tokenizer.token_end(),
                  bugfix_version);
    }
  }
}

}  // namespace base
