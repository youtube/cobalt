// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace file_util {

// Deny |permission| on the file |path|.
bool DenyFilePermission(const FilePath& path, mode_t permission) {
  struct stat stat_buf;
  if (stat(path.value().c_str(), &stat_buf) != 0)
    return false;
  stat_buf.st_mode &= ~permission;

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), stat_buf.st_mode));
  return rv == 0;
}

std::wstring FilePathAsWString(const FilePath& path) {
  return UTF8ToWide(path.value());
}
FilePath WStringAsFilePath(const std::wstring& path) {
  return FilePath(WideToUTF8(path));
}

bool MakeFileUnreadable(const FilePath& path) {
  return DenyFilePermission(path, S_IRUSR | S_IRGRP | S_IROTH);
}

bool MakeFileUnwritable(const FilePath& path) {
  return DenyFilePermission(path, S_IWUSR | S_IWGRP | S_IWOTH);
}

}  // namespace file_util