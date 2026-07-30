// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace base {

std::string NativeLibraryLoadError::ToString() const {
  return message;
}

NativeLibrary LoadNativeLibraryWithOptions(const FilePath&,
                                           const NativeLibraryOptions&,
                                           NativeLibraryLoadError*) {
  NOTREACHED() << "LoadNativeLibraryWithOptions called, but Starboard " 
               << "does not support dlopen. This call will fail.";
  return nullptr;
}

void UnloadNativeLibrary(NativeLibrary) {
  NOTREACHED() << "UnloadNativeLibrary was called, but Starboard " 
               << "does not support dlclose. This call will fail.";
  return;
}

void* GetFunctionPointerFromNativeLibrary(NativeLibrary,
                                          StringPiece) {
  NOTREACHED() << "GetFunctionPointerFromNativeLibrary was called, "
               << "but Starboard does not support dlsym. This call will fail.";
  return nullptr;
}

std::string GetNativeLibraryName(StringPiece name) {
  DCHECK(IsStringASCII(name));
  return StrCat({"lib", name, ".so"});
}

std::string GetLoadableModuleName(StringPiece name) {
  return GetNativeLibraryName(name);
}

}  // namespace base
