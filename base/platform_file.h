// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PLATFORM_FILE_H_
#define BASE_PLATFORM_FILE_H_

#include "build/build_config.h"
#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

class FilePath;

namespace base {

#if defined(OS_WIN)
typedef HANDLE PlatformFile;
const PlatformFile kInvalidPlatformFileValue = INVALID_HANDLE_VALUE;
#elif defined(OS_POSIX)
typedef int PlatformFile;
const PlatformFile kInvalidPlatformFileValue = -1;
#endif

enum PlatformFileFlags {
  PLATFORM_FILE_OPEN = 1,
  PLATFORM_FILE_CREATE = 2,
  PLATFORM_FILE_OPEN_ALWAYS = 4,    // May create a new file.
  PLATFORM_FILE_CREATE_ALWAYS = 8,  // May overwrite an old file.
  PLATFORM_FILE_READ = 16,
  PLATFORM_FILE_WRITE = 32,
  PLATFORM_FILE_EXCLUSIVE_READ = 64,  // EXCLUSIVE is opposite of Windows SHARE
  PLATFORM_FILE_EXCLUSIVE_WRITE = 128,
  PLATFORM_FILE_ASYNC = 256,
  PLATFORM_FILE_TEMPORARY = 512,        // Used on Windows only
  PLATFORM_FILE_HIDDEN = 1024,          // Used on Windows only
  PLATFORM_FILE_DELETE_ON_CLOSE = 2048
};

// Creates or opens the given file. If PLATFORM_FILE_OPEN_ALWAYS is used, and
// |created| is provided, |created| will be set to true if the file was created
// or to false in case the file was just opened.
PlatformFile CreatePlatformFile(const FilePath& name,
                                int flags,
                                bool* created);
// Deprecated.
PlatformFile CreatePlatformFile(const std::wstring& name,
                                int flags,
                                bool* created);

// Closes a file handle
bool ClosePlatformFile(PlatformFile file);

}  // namespace base

#endif  // BASE_PLATFORM_FILE_H_
