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

#include "cobalt/shell/app/shell_crash_reporter_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cobalt/shell/common/shell_switches.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "cobalt/shell/android/shell_descriptors.h"
#endif

namespace content {

ShellCrashReporterClient::ShellCrashReporterClient() {}
ShellCrashReporterClient::~ShellCrashReporterClient() {}

#if BUILDFLAG(IS_POSIX)
base::FilePath ShellCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

bool ShellCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir)) {
    return false;
  }
  base::FilePath crash_directory =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCrashDumpsDir);
  *crash_dir = std::move(crash_directory);
  return true;
}

void ShellCrashReporterClient::GetProductInfo(ProductInfo* product_info) {
  product_info->product_name = "content_shell";
  product_info->version = CONTENT_SHELL_VERSION;
}

bool ShellCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

}  // namespace content
