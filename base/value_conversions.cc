// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/value_conversions.h"

#include "base/file_path.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"

namespace base {

namespace {

// |Value| internally stores strings in UTF-8, so we have to convert from the
// system native code to UTF-8 and back.

std::string FilePathToUTF8(const FilePath& file_path) {
#if defined(OS_POSIX)
  return WideToUTF8(SysNativeMBToWide(file_path.value()));
#else
  return UTF16ToUTF8(file_path.value());
#endif
}

FilePath UTF8ToFilePath(const std::string& str) {
  FilePath::StringType result;
#if defined(OS_POSIX)
  result = SysWideToNativeMB(UTF8ToWide(str));
#elif defined(OS_WIN)
  result = UTF8ToUTF16(str);
#endif
  return FilePath(result);
}

}  // namespace

StringValue* CreateFilePathValue(const FilePath& in_value) {
  return new StringValue(FilePathToUTF8(in_value));
}

bool GetValueAsFilePath(const Value& value, FilePath* file_path) {
  std::string str;
  if (!value.GetAsString(&str))
    return false;
  if (file_path)
    *file_path = UTF8ToFilePath(str);
  return true;
}

}  // namespace base
