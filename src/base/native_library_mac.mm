// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"

namespace base {

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                std::string* error) {
  // dlopen() etc. open the file off disk.
  if (library_path.Extension() == "dylib" ||
      !file_util::DirectoryExists(library_path)) {
    void* dylib = dlopen(library_path.value().c_str(), RTLD_LAZY);
    if (!dylib)
      return NULL;
    NativeLibrary native_lib = new NativeLibraryStruct();
    native_lib->type = DYNAMIC_LIB;
    native_lib->dylib = dylib;
    return native_lib;
  }
  base::mac::ScopedCFTypeRef<CFURLRef> url(
      CFURLCreateFromFileSystemRepresentation(
          kCFAllocatorDefault,
          (const UInt8*)library_path.value().c_str(),
          library_path.value().length(),
          true));
  if (!url)
    return NULL;
  CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, url.get());
  if (!bundle)
    return NULL;

  NativeLibrary native_lib = new NativeLibraryStruct();
  native_lib->type = BUNDLE;
  native_lib->bundle = bundle;
  native_lib->bundle_resource_ref = CFBundleOpenBundleResourceMap(bundle);
  return native_lib;
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  if (library->type == BUNDLE) {
    CFBundleCloseBundleResourceMap(library->bundle,
                                   library->bundle_resource_ref);
    CFRelease(library->bundle);
  } else {
    dlclose(library->dylib);
  }
  delete library;
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  if (library->type == BUNDLE) {
    base::mac::ScopedCFTypeRef<CFStringRef> symbol_name(
        CFStringCreateWithCString(kCFAllocatorDefault, name,
                                  kCFStringEncodingUTF8));
    return CFBundleGetFunctionPointerForName(library->bundle, symbol_name);
  }
  return dlsym(library->dylib, name);
}

// static
string16 GetNativeLibraryName(const string16& name) {
  return name + ASCIIToUTF16(".dylib");
}

}  // namespace base
