// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <ostream>
#include <string>

#include "build/build_config.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/nix/xdg_util.h"

#if defined(OS_FREEBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#elif defined(OS_SOLARIS)
#include <stdlib.h>
#endif

namespace base {

#if defined(OS_LINUX)
const char kSelfExe[] = "/proc/self/exe";
#endif

// The name of this file relative to the source root. This is used for checking
// that the source checkout is in the correct place.
static const char kThisSourceFile[] = "base/base_paths_posix.cc";

bool PathProviderPosix(int key, FilePath* result) {
  FilePath path;
  switch (key) {
    case base::FILE_EXE:
    case base::FILE_MODULE: {  // TODO(evanm): is this correct?
#if defined(OS_LINUX)
      FilePath bin_dir;
      if (!file_util::ReadSymbolicLink(FilePath(kSelfExe), &bin_dir)) {
        NOTREACHED() << "Unable to resolve " << kSelfExe << ".";
        return false;
      }
      *result = bin_dir;
      return true;
#elif defined(OS_FREEBSD)
      int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
      char bin_dir[PATH_MAX + 1];
      size_t length = sizeof(bin_dir);
      // Upon return, |length| is the number of bytes written to |bin_dir|
      // including the string terminator.
      int error = sysctl(name, 4, bin_dir, &length, NULL, 0);
      if (error < 0 || length <= 1) {
        NOTREACHED() << "Unable to resolve path.";
        return false;
      }
      *result = FilePath(FilePath::StringType(bin_dir, length - 1));
      return true;
#elif defined(OS_SOLARIS)
      char bin_dir[PATH_MAX + 1];
      if (realpath(getexecname(), bin_dir) == NULL) {
        NOTREACHED() << "Unable to resolve " << getexecname() << ".";
        return false;
      }
      *result = FilePath(bin_dir);
      return true;
#elif defined(OS_OPENBSD)
      // There is currently no way to get the executable path on OpenBSD
      char *cpath;
      if ((cpath = getenv("CHROME_EXE_PATH")) != NULL)
        *result = FilePath(cpath);
      else
        *result = FilePath("/usr/local/chrome/chrome");
      return true;
#endif
    }
    case base::DIR_SOURCE_ROOT: {
      // Allow passing this in the environment, for more flexibility in build
      // tree configurations (sub-project builds, gyp --output_dir, etc.)
      scoped_ptr<base::Environment> env(base::Environment::Create());
      std::string cr_source_root;
      if (env->GetVar("CR_SOURCE_ROOT", &cr_source_root)) {
        path = FilePath(cr_source_root);
        if (file_util::PathExists(path.Append(kThisSourceFile))) {
          *result = path;
          return true;
        } else {
          DLOG(WARNING) << "CR_SOURCE_ROOT is set, but it appears to not "
                        << "point to the correct source root directory.";
        }
      }
      // On POSIX, unit tests execute two levels deep from the source root.
      // For example:  out/{Debug|Release}/net_unittest
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName();
        if (file_util::PathExists(path.Append(kThisSourceFile))) {
          *result = path;
          return true;
        }
      }
      // In a case of WebKit-only checkout, executable files are put into
      // <root of checkout>/out/{Debug|Release}, and we should return
      // <root of checkout>/Source/WebKit/chromium for DIR_SOURCE_ROOT.
      if (PathService::Get(base::DIR_EXE, &path)) {
        path = path.DirName().DirName().Append("Source/WebKit/chromium");
        if (file_util::PathExists(path.Append(kThisSourceFile))) {
          *result = path;
          return true;
        }
      }
      // If that failed (maybe the build output is symlinked to a different
      // drive) try assuming the current directory is the source root.
      if (file_util::GetCurrentDirectory(&path) &&
          file_util::PathExists(path.Append(kThisSourceFile))) {
        *result = path;
        return true;
      }
      DLOG(ERROR) << "Couldn't find your source root.  "
                  << "Try running from your chromium/src directory.";
      return false;
    }
    case base::DIR_CACHE:
      scoped_ptr<base::Environment> env(base::Environment::Create());
      FilePath cache_dir(base::nix::GetXDGDirectory(env.get(), "XDG_CACHE_HOME",
                                                    ".cache"));
      *result = cache_dir;
      return true;
  }
  return false;
}

}  // namespace base
