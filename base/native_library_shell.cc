// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"

namespace base {

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                std::string* error) {
  NOTIMPLEMENTED();
  return 0;
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  NOTIMPLEMENTED();
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
string16 GetNativeLibraryName(const string16& name) {
  NOTIMPLEMENTED();
  return string16();
}

}  // namespace base
