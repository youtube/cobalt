// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cobalt_main_delegate.h"

#include <iostream>

#include "base/path_service.h"
#include "cobalt/cobalt_content_browser_client.h"
#include "cobalt/cobalt_paths.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/web_test/browser/web_test_browser_main_runner.h"  // nogncheck
#include "content/web_test/browser/web_test_content_browser_client.h"  // nogncheck
#include "content/web_test/renderer/web_test_content_renderer_client.h"  // nogncheck
#endif

namespace {

void InitLogging(const base::CommandLine& command_line) {
  base::FilePath log_filename =
      command_line.GetSwitchValuePath(switches::kLogFile);
  if (log_filename.empty()) {
    base::PathService::Get(base::DIR_EXE, &log_filename);
    log_filename = log_filename.AppendASCII("cobalt.log");
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

}  // namespace

namespace cobalt {

CobaltMainDelegate::CobaltMainDelegate(bool is_content_browsertests)
    : content::ShellMainDelegate(is_content_browsertests) {}

CobaltMainDelegate::~CobaltMainDelegate() {}

absl::optional<int> CobaltMainDelegate::BasicStartupComplete() {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_ANDROID)
  content::Compositor::Initialize();
#endif

  InitLogging(command_line);

#if !BUILDFLAG(IS_ANDROID)
  if (switches::IsRunWebTestsSwitchPresent()) {
    const bool browser_process =
        command_line.GetSwitchValueASCII(switches::kProcessType).empty();
    if (browser_process) {
      web_test_runner_ = std::make_unique<content::WebTestBrowserMainRunner>();
      web_test_runner_->Initialize();
    }
  }
#endif

  RegisterCobaltPathProvider();

  return absl::nullopt;
}

content::ContentBrowserClient*
CobaltMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<CobaltContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace cobalt
