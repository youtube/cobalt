// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_NATIVE_LIBRARY_H_
#define BASE_SCOPED_NATIVE_LIBRARY_H_

#include "base/file_path.h"
#include "base/native_library.h"

namespace base {

// A class which encapsulates a base::NativeLibrary object available only in a
// scope.
// This class automatically unloads the loaded library in its destructor.
class ScopedNativeLibrary {
 public:
  explicit ScopedNativeLibrary(const FilePath& library_path) {
    library_ = base::LoadNativeLibrary(library_path);
  }

  ~ScopedNativeLibrary() {
    if (library_)
      base::UnloadNativeLibrary(library_);
  }

  void* GetFunctionPointer(const char* function_name) {
    if (!library_)
      return NULL;
    return base::GetFunctionPointerFromNativeLibrary(library_, function_name);
  }

 private:
  base::NativeLibrary library_;
  DISALLOW_COPY_AND_ASSIGN(ScopedNativeLibrary);
};

}  // namespace base

#endif  // BASE_SCOPED_NATIVE_LIBRARY_H_
