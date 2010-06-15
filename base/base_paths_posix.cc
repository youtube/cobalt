// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is really Posix minus Mac. Mac code is in base_paths_mac.mm.

#include "base/base_paths.h"

#include <unistd.h>
#if defined(OS_FREEBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include "base/env_var.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "base/xdg_util.h"

namespace base {

#if defined(OS_LINUX)
const char kSelfExe[] = "/proc/self/exe";
#elif defined(OS_SOLARIS)
const char kSelfExe[] = getexecname();
#endif

bool PathProviderPosix(int key, FilePath* result) {
  FilePath path;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {  // TODO(evanm): is this correct?
#if defined(OS_LINUX)
      char bin_dir[PATH_MAX + 1];
      int bin_dir_size = readlink(kSelfExe, bin_dir, PATH_MAX);
      if (bin_dir_size < 0 || bin_dir_size > PATH_MAX) {
        NOTREACHED() << "Unable to resolve " << kSelfExe << ".";
        return false;
      }
      bin_dir[bin_dir_size] = 0;
      *result = FilePath(bin_dir);
      return true;
#elif defined(OS_FREEBSD)
      int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
      char bin_dir[PATH_MAX + 1];
      size_t length = sizeof(bin_dir);
      int error = sysctl(name, 4, bin_dir, &length, NULL, 0);
      if (error < 0 || length == 0 || strlen(bin_dir) == 0) {
        NOTREACHED() << "Unable to resolve path.";
        return false;
      }
      bin_dir[strlen(bin_dir)] = 0;
      *result = FilePath(bin_dir);
      return true;
#endif
    }
    case base::DIR_SOURCE_ROOT: {
      // Allow passing this in the environment, for more flexibility in build
      // tree configurations (sub-project builds, gyp --output_dir, etc.)
      scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
      std::string cr_source_root;
      if (env->GetEnv("CR_SOURCE_ROOT", &cr_source_root)) {
        path = FilePath(cr_source_root);
        if (file_util::PathExists(path.Append("base/base_paths_posix.cc"))) {
          *result = path;
          return true;
        } else {
          LOG(WARNING) << "CR_SOURCE_ROOT is set, but it appears to not "
                       << "point to the correct source root directory.";
        }
      }
      // On POSIX, unit tests execute two levels deep from the source root.
      // For example:  sconsbuild/{Debug|Release}/net_unittest
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName();
        if (file_util::PathExists(path.Append("base/base_paths_posix.cc"))) {
          *result = path;
          return true;
        }
      }
      // In a case of WebKit-only checkout, executable files are put into
      // WebKit/out/{Debug|Release}, and we should return WebKit/WebKit/chromium
      // for DIR_SOURCE_ROOT.
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName().Append("WebKit/chromium");
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
    }
    case base::DIR_USER_CACHE:
      scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
      FilePath cache_dir(base::GetXDGDirectory(env.get(), "XDG_CACHE_HOME",
                                               ".cache"));
      *result = cache_dir;
      return true;
  }
  return false;
}

}  // namespace base
