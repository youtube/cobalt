// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <unistd.h>

#include "base/android/jni_android.h"
#include "base/android/path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace {

const char kSelfExe[] = "/proc/self/exe";

}  // namespace

namespace base {

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
    case base::FILE_MODULE: {
      // dladdr didn't work in Android as only the file name was returned.
      NOTIMPLEMENTED();
      return false;
    }
    case base::DIR_MODULE: {
      *result = FilePath(base::android::GetNativeLibraryDirectory());
      return true;
    }
    case base::DIR_SOURCE_ROOT: {
      // This const is only used for tests.
      *result = FilePath(base::android::GetExternalStorageDirectory());
      return true;
    }
    case base::DIR_CACHE: {
      *result = FilePath(base::android::GetCacheDirectory());
      return true;
    }
    case base::DIR_ANDROID_APP_DATA: {
      *result = FilePath(base::android::GetDataDirectory());
      return true;
    }
    case base::DIR_HOME: {
      *result = file_util::GetHomeDir();
      return true;
    }
    case base::DIR_ANDROID_EXTERNAL_STORAGE: {
      *result = FilePath(base::android::GetExternalStorageDirectory());
      return true;
    }
    default: {
      // Note: the path system expects this function to override the default
      // behavior. So no need to log an error if we don't support a given
      // path. The system will just use the default.
      return false;
    }
  }
}

}  // namespace base
