// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/android_os.h"

namespace base {

const char kSelfExe[] = "/proc/self/exe";

bool PathProviderAndroid(int key, FilePath* result) {
  switch (key) {
    case base::FILE_EXE: {
      char bin_dir[PATH_MAX + 1];
      int bin_dir_size = readlink(kSelfExe, bin_dir, PATH_MAX);
      if (bin_dir_size < 0 || bin_dir_size > PATH_MAX) {
        NOTREACHED() << "Unable to resolve " << kSelfExe << ".";
        return false;
      }
      bin_dir[bin_dir_size] = 0;
      *result = FilePath(bin_dir);
      return true;
    }
    case base::FILE_MODULE:
      // TODO(port): Find out whether we can use dladdr to implement this, and
      // then use DIR_MODULE's default implementation in base_file.cc.
      NOTIMPLEMENTED();
      return false;
    case base::DIR_MODULE: {
      AndroidOS* aos = AndroidOS::GetSharedInstance();
      DCHECK(aos);
      *result = aos->GetLibDirectory();
      return true;
    }
    case base::DIR_SOURCE_ROOT:
      // This const is only used for tests. Files in this directory are pushed
      // to the device via test script.
      *result = FilePath(FILE_PATH_LITERAL("/data/local/tmp/"));
      return true;
    case base::DIR_CACHE: {
      AndroidOS* aos = AndroidOS::GetSharedInstance();
      DCHECK(aos);
      *result = aos->GetCacheDirectory();
      return true;
    }
    default:
      // Note: the path system expects this function to override the default
      // behavior. So no need to log an error if we don't support a given
      // path. The system will just use the default.
      return false;
  }
}

}  // namespace base
