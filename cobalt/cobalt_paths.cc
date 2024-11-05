// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/cobalt_paths.h"

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#endif

namespace cobalt {

namespace {

bool GetDefaultUserDataDirectory(base::FilePath* result) {
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.Append("cobalt");
#elif BUILDFLAG(IS_ANDROID)
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, result));
  *result = result->Append(FILE_PATH_LITERAL("cobalt"));
#else
  NOTIMPLEMENTED();
  return false;
#endif
  return true;
}

}  // namespace

class CobaltPathProvider {
 public:
  static void CreateDir(const base::FilePath& path) {
    base::ScopedAllowBlocking allow_io;
    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
  }
};

bool CobaltPathProvider(int key, base::FilePath* result) {
  base::FilePath cur;

  switch (key) {
    case SHELL_DIR_USER_DATA: {
      bool rv = GetDefaultUserDataDirectory(result);
      if (rv) {
        CobaltPathProvider::CreateDir(*result);
      }
      return rv;
    }
    default:
      return false;
  }
}

void RegisterCobaltPathProvider() {
  base::PathService::RegisterProvider(CobaltPathProvider, SHELL_PATH_START,
                                      SHELL_PATH_END);
}

}  // namespace cobalt
