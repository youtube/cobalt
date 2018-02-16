// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_H_
#define BASE_BASE_PATHS_H_

// This file declares path keys for the base module.  These can be used with
// the PathService to access various special directories and files.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#elif defined(OS_ANDROID) || defined(__LB_ANDROID__)
#include "base/base_paths_android.h"
#elif defined(OS_STARBOARD)
#include "base/base_paths_starboard.h"
#endif

#if defined(OS_POSIX)
#include "base/base_paths_posix.h"
#endif

namespace base {

enum BasePathKey {
  PATH_START = 0,

  DIR_CURRENT,  // current directory
  DIR_EXE,      // directory containing FILE_EXE
  DIR_MODULE,   // directory containing FILE_MODULE
  DIR_TEMP,     // temporary directory
  FILE_EXE,     // Path and filename of the current executable.
  FILE_MODULE,  // Path and filename of the module containing the code for the
                // PathService (which could differ from FILE_EXE if the
                // PathService were compiled into a shared object, for example).
  DIR_SOURCE_ROOT,  // Returns the root of the source tree.  Only for utility
                    // code that runs on the host to locate various resources.
                    // Not available in release builds.
  DIR_USER_DESKTOP,  // The current user's Desktop.

  DIR_TEST_DATA,  // Directory holding static input for testing.
                  // Not available in release builds.

  PATH_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_H_
