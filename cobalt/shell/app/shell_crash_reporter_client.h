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

#ifndef COBALT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
#define COBALT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_

#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace content {

class ShellCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  ShellCrashReporterClient();

  ShellCrashReporterClient(const ShellCrashReporterClient&) = delete;
  ShellCrashReporterClient& operator=(const ShellCrashReporterClient&) = delete;

  ~ShellCrashReporterClient() override;

#if BUILDFLAG(IS_POSIX)
  base::FilePath GetReporterLogFilename() override;
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;

  void GetProductInfo(ProductInfo* product_info) override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;
};

}  // namespace content

#endif  // COBALT_SHELL_APP_SHELL_CRASH_REPORTER_CLIENT_H_
