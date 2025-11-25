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

#include "cobalt/testing/browser_tests/content_browser_test.h"

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"
#include "cobalt/testing/browser_tests/shell/test_shell.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/test/test_content_client.h"
#include "ui/events/platform/platform_event_source.h"

#include "base/functional/callback.h"
#include "base/threading/thread.h"
#include "starboard/event.h"
#include "starboard/system.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/foundation_util.h"
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#include "ui/base/ime/init/input_method_initializer.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/test/network_connection_change_simulator.h"
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS)
#include "ui/views/test/widget_test_api.h"  // nogncheck
#endif

namespace content {

static ContentBrowserTest* g_instance = nullptr;

void SbEventHandle(const SbEvent* event) {
  // This is a global function that is passed to SbRunStarboardMain.
  // We need to route this to the ContentBrowserTest instance.
  if (g_instance) {
    g_instance->OnStarboardEvent(event);
  }
}

ContentBrowserTest::ContentBrowserTest()
    : starboard_setup_complete_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  g_instance = this;
  // In content browser tests ContentBrowserTestContentBrowserClient must be
  // used. ContentBrowserTestContentBrowserClient's constructor (and destructor)
  // uses this same function to change the ContentBrowserClient.
  ContentClient::SetCanChangeContentBrowserClientForTesting(false);

  CreateTestServer(GetTestDataFilePath());

  // Fail as quickly as possible during tests, rather than attempting to reset
  // accessibility and continue when unserialization fails.
  RenderFrameHostImpl::max_accessibility_resets_ = 0;
}

ContentBrowserTest::~ContentBrowserTest() {
  if (g_instance == this) {
    g_instance = nullptr;
  }
}

void ContentBrowserTest::OnStarboardEvent(const SbEvent* event) {
  if (event->type == kSbEventTypePreload || event->type == kSbEventTypeStart) {
    BrowserTestBase::SetUp();
    starboard_setup_complete_.Signal();
  }
}

void ContentBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);

#if BUILDFLAG(IS_MAC)
  // See InProcessBrowserTest::PrepareTestCommandLine().
  base::FilePath subprocess_path;
  base::PathService::Get(base::FILE_EXE, &subprocess_path);
  subprocess_path = subprocess_path.DirName().DirName();
  DCHECK_EQ(subprocess_path.BaseName().value(), "Contents");
  subprocess_path = subprocess_path.Append(
      "Frameworks/Content Shell Framework.framework/Helpers/Content Shell "
      "Helper.app/Contents/MacOS/Content Shell Helper");
  command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                 subprocess_path);
#endif

#if defined(USE_AURA) && defined(TOOLKIT_VIEWS) && !BUILDFLAG(IS_CASTOS)
  // https://crbug.com/695054: Ignore window activation/deactivation to make
  // the Chrome-internal focus unaffected by OS events caused by running tests
  // in parallel.
  views::DisableActivationChangeHandlingForTests();
#endif

  // LinuxInputMethodContextFactory has to be initialized.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  ui::InitializeInputMethodForTesting();
#endif

  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);

  // Initialize and run the SB Application player on a separate thread.
  starboard_thread_ = std::make_unique<base::Thread>("StarboardMainThread");
  starboard_thread_->Start();

  // Create a mutable copy of the command line arguments for SbRunStarboardMain.
  // The ownership of this data will be passed to the task posted to the
  // starboard_thread_, which will be responsible for cleaning it up.
  auto argv_vector = std::make_unique<std::vector<char*>>();
  for (const auto& arg : command_line->argv()) {
    char* arg_copy = new char[arg.size() + 1];
    strcpy(arg_copy, arg.c_str());
    argv_vector->push_back(arg_copy);
  }

  starboard_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<std::vector<char*>> argv_vector) {
            SbRunStarboardMain(argv_vector->size(), argv_vector->data(),
                               SbEventHandle);
            // Clean up the allocated strings.
            for (char* arg : *argv_vector) {
              delete[] arg;
            }
          },
          std::move(argv_vector)));

  starboard_setup_complete_.Wait();
}

void ContentBrowserTest::TearDown() {
  BrowserTestBase::TearDown();

  // Stop the Starboard application and join the thread.
  SbSystemRequestStop(0);
  if (starboard_thread_) {
    starboard_thread_->Stop();
  }

// LinuxInputMethodContextFactory has to be shutdown.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  ui::ShutdownInputMethodForTesting();
#endif
}

void ContentBrowserTest::PreRunTestOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS)
  NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.InitializeChromeosConnectionType();
#endif

  CHECK_EQ(Shell::windows().size(), 1u);
  shell_ = Shell::windows()[0];
  SetInitialWebContents(shell_->web_contents());

#if BUILDFLAG(IS_MAC)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  pool_ = new base::mac::ScopedNSAutoreleasePool;
#endif

  // Pump startup related events.
  DCHECK(base::CurrentUIThread::IsSet());
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_MAC)
  pool_->Recycle();
#endif

  pre_run_test_executed_ = true;
}

void ContentBrowserTest::PostRunTestOnMainThread() {
  // This code is failing when the test is overriding PreRunTestOnMainThread()
  // without the required call to ContentBrowserTest::PreRunTestOnMainThread().
  // This is a common error causing a crash on MAC.
  DCHECK(pre_run_test_executed_);

#if BUILDFLAG(IS_MAC)
  pool_->Recycle();
#endif

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
  return base::FilePath(FILE_PATH_LITERAL("cobalt/testing/browser_tests/data"));
}

}  // namespace content
