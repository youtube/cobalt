// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_mac.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/string_util.h"

namespace {

void GetNSExecutablePath(FilePath* path) {
  DCHECK(path);
  // Executable path can have relative references ("..") depending on
  // how the app was launched.
  uint32_t executable_length = 0;
  _NSGetExecutablePath(NULL, &executable_length);
  DCHECK_GT(executable_length, 1u);
  std::string executable_path;
  int rv = _NSGetExecutablePath(WriteInto(&executable_path, executable_length),
                                &executable_length);
  DCHECK_EQ(rv, 0);
  *path = FilePath(executable_path);
}

// Returns true if the module for |address| is found. |path| will contain
// the path to the module. Note that |path| may not be absolute.
bool GetModulePathForAddress(FilePath* path,
                             const void* address) WARN_UNUSED_RESULT;

bool GetModulePathForAddress(FilePath* path, const void* address) {
  Dl_info info;
  if (dladdr(address, &info) == 0)
    return false;
  *path = FilePath(info.dli_fname);
  return true;
}

}  // namespace

namespace base {

bool PathProviderMac(int key, FilePath* result) {
  switch (key) {
    case base::FILE_EXE:
      GetNSExecutablePath(result);
      return true;
    case base::FILE_MODULE:
      return GetModulePathForAddress(result,
          reinterpret_cast<const void*>(&base::PathProviderMac));
    case base::DIR_CACHE:
      return base::mac::GetUserDirectory(NSCachesDirectory, result);
    case base::DIR_APP_DATA:
      return base::mac::GetUserDirectory(NSApplicationSupportDirectory, result);
    case base::DIR_SOURCE_ROOT: {
      // Go through PathService to catch overrides.
      if (!PathService::Get(base::FILE_EXE, result))
        return false;

      // Start with the executable's directory.
      *result = result->DirName();
      if (base::mac::AmIBundled()) {
        // The bundled app executables (Chromium, TestShell, etc) live five
        // levels down, eg:
        // src/xcodebuild/{Debug|Release}/Chromium.app/Contents/MacOS/Chromium
        *result = result->DirName().DirName().DirName().DirName().DirName();
      } else {
        // Unit tests execute two levels deep from the source root, eg:
        // src/xcodebuild/{Debug|Release}/base_unittests
        *result = result->DirName().DirName();
      }
      return true;
    }
    default:
      return false;
  }
}

}  // namespace base
