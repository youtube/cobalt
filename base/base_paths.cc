// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace base {

bool PathProvider(int key, FilePath* result) {
  // NOTE: DIR_CURRENT is a special case in PathService::Get

  switch (key) {
    case DIR_EXE:
      if (!PathService::Get(FILE_EXE, result))
        return false;
      *result = result->DirName();
      return true;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case DIR_MODULE:
      if (!PathService::Get(FILE_MODULE, result))
        return false;
      *result = result->DirName();
      return true;
    case DIR_ASSETS:
      return PathService::Get(DIR_MODULE, result);
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case DIR_TEMP:
      return GetTempDir(result);
    case DIR_HOME:
      *result = GetHomeDir();
      return true;
    case base::DIR_SRC_TEST_DATA_ROOT:
      // This is only used by tests and overridden by each platform.
      NOTREACHED();
      return false;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case DIR_OUT_TEST_DATA_ROOT:
      // On most platforms test binaries are run directly from the build-output
      // directory, so return the directory containing the executable.
      return PathService::Get(DIR_MODULE, result);
#endif  // !BUILDFLAG(IS_FUCHSIA)  && !BUILDFLAG(IS_IOS)
    case DIR_GEN_TEST_DATA_ROOT:
      if (!PathService::Get(DIR_OUT_TEST_DATA_ROOT, result)) {
        return false;
      }
      *result = result->Append(FILE_PATH_LITERAL("gen"));
      return true;
    case DIR_TEST_DATA: {
      FilePath test_data_path;
      if (!PathService::Get(DIR_SRC_TEST_DATA_ROOT, &test_data_path)) {
        return false;
      }
      test_data_path = test_data_path.Append(FILE_PATH_LITERAL("base"));
      test_data_path = test_data_path.Append(FILE_PATH_LITERAL("test"));
      test_data_path = test_data_path.Append(FILE_PATH_LITERAL("data"));
      if (!PathExists(test_data_path))  // We don't want to create this.
        return false;
      *result = test_data_path;
      return true;
    }
  }

  return false;
}

}  // namespace base
