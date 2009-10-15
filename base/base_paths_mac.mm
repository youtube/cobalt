// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_mac.h"

#import <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#include "base/string_util.h"

namespace base {

namespace {

// TODO(akalin): Export this function somewhere and use it in
// chrome_paths_mac.mm and mac_util.mm.  This is tricky because
// NSSearchPathDirectory is declared in an Objective C header so we
// cannot put it in one of the usual locations (where pure C++ files
// would include them).
bool GetUserDirectory(NSSearchPathDirectory directory, FilePath* result) {
  NSArray* dirs =
      NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES);
  if ([dirs count] < 1) {
    return false;
  }
  NSString* path = [dirs objectAtIndex:0];
  *result = FilePath([path fileSystemRepresentation]);
  return true;
}

}  // namespace

bool PathProviderMac(int key, FilePath* result) {
  std::string cur;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {
      // Executable path can have relative references ("..") depending on
      // how the app was launched.
      uint32_t executable_length = 0;
      _NSGetExecutablePath(NULL, &executable_length);
      DCHECK_GT(executable_length, 1u);
      char* executable = WriteInto(&cur, executable_length);
      int rv = _NSGetExecutablePath(executable, &executable_length);
      DCHECK_EQ(rv, 0);
      DCHECK(!cur.empty());
      break;
    }
    case base::DIR_CACHE:
      return GetUserDirectory(NSCachesDirectory, result);
    case base::DIR_APP_DATA:
      return GetUserDirectory(NSApplicationSupportDirectory, result);
    case base::DIR_SOURCE_ROOT: {
      PathService::Get(base::DIR_EXE, result);
      if (mac_util::AmIBundled()) {
        // The bundled app executables (Chromium, TestShell, etc) live five
        // levels down, eg:
        // src/xcodebuild/{Debug|Release}/Chromium.app/Contents/MacOS/Chromium.
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

  *result = FilePath(cur);
  return true;
}

}  // namespace base
