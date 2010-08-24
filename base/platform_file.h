// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PLATFORM_FILE_H_
#define BASE_PLATFORM_FILE_H_
#pragma once

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

// TODO(dumi): add more specific error codes for CreatePlatformFile().
// TODO(dumi): add more error codes as we add new methods to FileUtilProxy.
enum PlatformFileErrors {
  PLATFORM_FILE_OK = 0,
  PLATFORM_FILE_ERROR = -1
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

// Use this class to pass ownership of a PlatformFile to a receiver that may or
// may not want to accept it.  This class does not own the storage for the
// PlatformFile.
//
// EXAMPLE:
//
//  void MaybeProcessFile(PassPlatformFile pass_file) {
//    if (...) {
//      PlatformFile file = pass_file.ReleaseValue();
//      // Now, we are responsible for closing |file|.
//    }
//  }
//
//  void OpenAndMaybeProcessFile(const FilePath& path) {
//    PlatformFile file = CreatePlatformFile(path, ...);
//    MaybeProcessFile(PassPlatformFile(&file));
//    if (file != kInvalidPlatformFileValue)
//      ClosePlatformFile(file);
//  }
//
class PassPlatformFile {
 public:
  explicit PassPlatformFile(PlatformFile* value) : value_(value) {
  }

  // Called to retrieve the PlatformFile stored in this object.  The caller
  // gains ownership of the PlatformFile and is now responsible for closing it.
  // Any subsequent calls to this method will return an invalid PlatformFile.
  PlatformFile ReleaseValue() {
    PlatformFile temp = *value_;
    *value_ = kInvalidPlatformFileValue;
    return temp;
  }

 private:
  PlatformFile* value_;
};

}  // namespace base

#endif  // BASE_PLATFORM_FILE_H_
