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

#include "cobalt/testing/shell/content_browser_test.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/testing/shell/content_browser_test_content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/test/test_content_client.h"
#include "ui/events/platform/platform_event_source.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS)
#include "ui/views/test/widget_test_api.h"  // nogncheck
#endif

namespace content {

ContentBrowserTest::ContentBrowserTest() {
  // In content browser tests ContentBrowserTestContentBrowserClient must be
  // used. ContentBrowserTestContentBrowserClient's constructor (and destructor)
  // uses this same function to change the ContentBrowserClient.
  ContentClient::SetCanChangeContentBrowserClientForTesting(false);
  CreateTestServer(GetTestDataFilePath());
}

ContentBrowserTest::~ContentBrowserTest() {}

void ContentBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CASTOS)
  // https://crbug.com/695054: Ignore window activation/deactivation to make
  // the Chrome-internal focus unaffected by OS events caused by running tests
  // in parallel.
  views::DisableActivationChangeHandlingForTests();
#endif

  // LinuxInputMethodContextFactory has to be initialized.
#if BUILDFLAG(IS_LINUX)
  ui::InitializeInputMethodForTesting();
#endif

  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);

  BrowserTestBase::SetUp();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  // LinuxInputMethodContextFactory has to be shutdown.
#if BUILDFLAG(IS_LINUX)
  ui::ShutdownInputMethodForTesting();
#endif
}

void ContentBrowserTest::PreRunTestOnMainThread() {
  CHECK_EQ(Shell::windows().size(), 1u);
  shell_ = Shell::windows()[0];
  SetInitialWebContents(shell_->web_contents());

  // Pump startup related events.
  DCHECK(base::CurrentUIThread::IsSet());
  base::RunLoop().RunUntilIdle();

  pre_run_test_executed_ = true;
}

void ContentBrowserTest::PostRunTestOnMainThread() {
  // This code is failing when the test is overriding PreRunTestOnMainThread()
  // without the required call to ContentBrowserTest::PreRunTestOnMainThread().
  // This is a common error causing a crash on MAC.
  DCHECK(pre_run_test_executed_);

  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }

  Shell::Shutdown();
}

Shell* ContentBrowserTest::CreateBrowser() {
  return Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->browser_context(),
      GURL(url::kAboutBlankURL), nullptr, gfx::Size());
}

Shell* ContentBrowserTest::CreateOffTheRecordBrowser() {
  return Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->off_the_record_browser_context(),
      GURL(url::kAboutBlankURL), nullptr, gfx::Size());
}

base::FilePath ContentBrowserTest::GetTestDataFilePath() {
  return base::FilePath(FILE_PATH_LITERAL("content/test/data"));
}

}  // namespace content
