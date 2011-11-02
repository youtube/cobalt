// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/value_conversions.h"

#include "base/file_path.h"
#include "base/values.h"

namespace base {

// |Value| internally stores strings in UTF-8, so we have to convert from the
// system native code to UTF-8 and back.
StringValue* CreateFilePathValue(const FilePath& in_value) {
  return new StringValue(in_value.AsUTF8Unsafe());
}

bool GetValueAsFilePath(const Value& value, FilePath* file_path) {
  std::string str;
  if (!value.GetAsString(&str))
    return false;
  if (file_path)
    *file_path = FilePath::FromUTF8Unsafe(str);
  return true;
}

}  // namespace base
