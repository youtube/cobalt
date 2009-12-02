// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is really Posix minus Mac. Mac code is in base_paths_mac.mm.

#include "base/base_paths.h"

#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/sys_string_conversions.h"

namespace base {

#if defined(OS_LINUX)
const char kSelfExe[] = "/proc/self/exe";
#elif defined(OS_FREEBSD)
const char kSelfExe[] = "/proc/curproc/file";
#endif

bool PathProviderPosix(int key, FilePath* result) {
  FilePath path;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {  // TODO(evanm): is this correct?
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
    case base::DIR_SOURCE_ROOT:
      // On POSIX, unit tests execute two levels deep from the source root.
      // For example:  sconsbuild/{Debug|Release}/net_unittest
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName();
        if (file_util::PathExists(path.Append("base/base_paths_posix.cc"))) {
          *result = path;
          return true;
        }
      }
      // If that failed (maybe the build output is symlinked to a different
      // drive) try assuming the current directory is the source root.
      if (file_util::GetCurrentDirectory(&path) &&
          file_util::PathExists(path.Append("base/base_paths_posix.cc"))) {
        *result = path;
        return true;
      }
      LOG(ERROR) << "Couldn't find your source root.  "
                 << "Try running from your chromium/src directory.";
      return false;
    case base::DIR_USER_CACHE:
      scoped_ptr<base::EnvironmentVariableGetter> env(
          base::EnvironmentVariableGetter::Create());
      FilePath cache_dir(base::GetXDGDirectory(env.get(), "XDG_CACHE_HOME",
                                               ".cache"));
      *result = cache_dir;
      return true;
  }
  return false;
}

}  // namespace base
