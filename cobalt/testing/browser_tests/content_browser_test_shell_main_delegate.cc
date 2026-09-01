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

#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"

#include <optional>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/base_paths_android.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#endif

#include "base/test/task_environment.h"
#include "cobalt/testing/browser_tests/browser/shell_content_browser_test_client.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"

namespace content {

ContentBrowserTestShellMainDelegate::ContentBrowserTestShellMainDelegate()
    : ShellMainTestDelegate() {}

ContentBrowserTestShellMainDelegate::~ContentBrowserTestShellMainDelegate() =
    default;

void ContentBrowserTestShellMainDelegate::CreateThreadPool(
    std::string_view name) {
  // Injects a test TaskTracker to watch for long-running tasks and produce a
  // useful timeout message in order to find the cause of flaky timeout tests.
  base::test::TaskEnvironment::CreateThreadPool();
}

ContentBrowserClient*
ContentBrowserTestShellMainDelegate::CreateContentBrowserClient() {
#if BUILDFLAG(IS_ANDROID)
  // Skia needs cobalt_android_fonts.xml to exist on Android, but tests
  // don't run CobaltActivity to copy it. Copy the OS config.
  base::FilePath app_data_dir;
  if (base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_dir)) {
    base::FilePath storage_dir = app_data_dir.Append("storage");
    base::ScopedAllowBlocking allow_blocking;
    base::CreateDirectory(storage_dir);
    base::FilePath xml_path = storage_dir.Append("cobalt_android_fonts.xml");
    if (!base::PathExists(xml_path)) {
      base::CopyFile(base::FilePath("/system/etc/fonts.xml"), xml_path);
    }
  }
#endif

  browser_client_ = std::make_unique<ContentBrowserTestContentBrowserClient>();
  return browser_client_.get();
}

}  // namespace content
