// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/common/shell_paths.h"

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#endif

namespace content {

namespace {

bool GetDefaultUserDataDirectory(base::FilePath* result) {
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.Append("content_shell");
#elif BUILDFLAG(IS_APPLE)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, result));
  *result = result->Append("Chromium Content Shell");
#elif BUILDFLAG(IS_ANDROID)
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, result));
  *result = result->Append(FILE_PATH_LITERAL("content_shell"));
#else
  NOTIMPLEMENTED();
  return false;
#endif
  return true;
}

}  // namespace

class ShellPathProvider {
 public:
  static void CreateDir(const base::FilePath& path) {
    base::ScopedAllowBlocking allow_io;
    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
  }
};

bool ShellPathProvider(int key, base::FilePath* result) {
  base::FilePath cur;

  switch (key) {
    case SHELL_DIR_USER_DATA: {
      base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
        if (cmd_line->HasSwitch(switches::kContentShellDataPath)) {
          path_ = cmd_line->GetSwitchValuePath(switches::kContentShellDataPath);
          if (base::DirectoryExists(path_) || base::CreateDirectory(path_))  {
            // BrowserContext needs an absolute path, which we would normally get via
            // PathService. In this case, manually ensure the path is absolute.
            if (!path_.IsAbsolute())
              path_ = base::MakeAbsoluteFilePath(path_);
            if (!path_.empty()) {
              *result = path;
              return true;
            }
          } else {
            LOG(WARNING) << "Unable to create data-path directory: " << path_.value();
          }
        }
      }
      bool rv = GetDefaultUserDataDirectory(result);
      if (rv) {
        ShellPathProvider::CreateDir(*result);
      }
      return rv;
    }
    default:
      return false;
  }
}

void RegisterShellPathProvider() {
  base::PathService::RegisterProvider(ShellPathProvider, SHELL_PATH_START,
                                      SHELL_PATH_END);
}

}  // namespace content
