// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/gtest_xml_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_support_ios.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/common/url_schemes.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/app/android/content_main_android.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/base_switches.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_types.h"

// To avoid conflicts with the macro from the Windows SDK...
#undef GetCommandLine
#elif BUILDFLAG(IS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif

namespace content {

namespace {

// Tests with this prefix run before the same test without it, and use the same
// profile. i.e. Foo.PRE_Test runs and then Foo.Test. This allows writing tests
// that span browser restarts.
const char kPreTestPrefix[] = "PRE_";

const char kManualTestPrefix[] = "MANUAL_";

TestLauncherDelegate* g_launcher_delegate = nullptr;

// The global ContentMainParams config to be copied in each test.
//
// Note that ContentMain is not run on Android in the test process, and is run
// via java for child processes. So this ContentMainParams is not used directly
// there. But it is still used to stash parameters for the test.
const ContentMainParams* g_params = nullptr;

void PrintUsage() {
  fprintf(stdout,
          "Runs tests using the gtest framework, each batch of tests being\n"
          "run in their own process. Supported command-line flags:\n"
          "\n"
          " Common flags:\n"
          "  --gtest_filter=...\n"
          "    Runs a subset of tests (see --gtest_help for more info).\n"
          "\n"
          "  --help\n"
          "    Shows this message.\n"
          "\n"
          "  --gtest_help\n"
          "    Shows the gtest help message.\n"
          "\n"
          "  --test-launcher-jobs=N\n"
          "    Sets the number of parallel test jobs to N.\n"
          "\n"
          "  --single-process-tests\n"
          "    Runs the tests and the launcher in the same process. Useful\n"
          "    for debugging a specific test in a debugger.\n"
          "\n"
          " Other flags:\n"
          "  --test-launcher-retry-limit=N\n"
          "    Sets the limit of test retries on failures to N.\n"
          "\n"
          "  --test-launcher-summary-output=PATH\n"
          "    Saves a JSON machine-readable summary of the run.\n"
          "\n"
          "  --test-launcher-print-test-stdio=auto|always|never\n"
          "    Controls when full test output is printed.\n"
          "    auto means to print it when the test failed.\n"
          "\n"
          "  --test-launcher-total-shards=N\n"
          "    Sets the total number of shards to N.\n"
          "\n"
          "  --test-launcher-shard-index=N\n"
          "    Sets the shard index to run to N (from 0 to TOTAL - 1).\n"
          "\n"
          "  --test-launcher-print-temp-leaks\n"
          "    Prints information about leaked files and/or directories in\n"
          "    child process's temporary directories (Windows and macOS).\n");
}

// Implementation of base::TestLauncherDelegate. This is also a test launcher,
// wrapping a lower-level test launcher with content-specific code.
class WrapperTestLauncherDelegate : public base::TestLauncherDelegate {
 public:
  explicit WrapperTestLauncherDelegate(
      content::TestLauncherDelegate* launcher_delegate)
      : launcher_delegate_(launcher_delegate) {
    run_manual_tests_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kRunManualTestsFlag);
  }

  WrapperTestLauncherDelegate(const WrapperTestLauncherDelegate&) = delete;
  WrapperTestLauncherDelegate& operator=(const WrapperTestLauncherDelegate&) =
      delete;

  // base::TestLauncherDelegate:
  bool GetTests(std::vector<base::TestIdentifier>* output) override;

  base::CommandLine GetCommandLine(const std::vector<std::string>& test_names,
                                   const base::FilePath& temp_dir,
                                   base::FilePath* output_file) override;

  size_t GetBatchSize() override;

  std::string GetWrapper() override;

  int GetLaunchOptions() override;

  base::TimeDelta GetTimeout() override;

  bool ShouldRunTest(const base::TestIdentifier& test) override;

 private:
  // Relays timeout notification from the TestLauncher (by way of a
  // ProcessLifetimeObserver) to the caller's content::TestLauncherDelegate.
  void OnTestTimedOut(const base::CommandLine& command_line) override;

  // Delegate additional TestResult processing.
  void ProcessTestResults(std::vector<base::TestResult>& test_results,
                          base::TimeDelta elapsed_time) override;

  raw_ptr<content::TestLauncherDelegate> launcher_delegate_;

  bool run_manual_tests_ = false;
};

bool WrapperTestLauncherDelegate::GetTests(
    std::vector<base::TestIdentifier>* output) {
  *output = base::GetCompiledInTests();
  return true;
}

bool IsPreTestName(const std::string& test_name) {
  return test_name.find(kPreTestPrefix) != std::string::npos;
}

size_t WrapperTestLauncherDelegate::GetBatchSize() {
  return 1u;
}

base::CommandLine WrapperTestLauncherDelegate::GetCommandLine(
    const std::vector<std::string>& test_names,
    const base::FilePath& temp_dir,
    base::FilePath* output_file) {
  DCHECK_EQ(1u, test_names.size());
  std::string test_name(test_names.front());
  // Chained pre tests must share the same temp directory,
  // TestLauncher should guarantee that for the delegate.
  base::FilePath user_data_dir = temp_dir.AppendASCII("user_data");
  CreateDirectory(user_data_dir);
  base::CommandLine cmd_line(*base::CommandLine::ForCurrentProcess());
  launcher_delegate_->PreRunTest();
  const std::string user_data_dir_switch =
      launcher_delegate_->GetUserDataDirectoryCommandLineSwitch();
  if (!user_data_dir_switch.empty())
    cmd_line.AppendSwitchPath(user_data_dir_switch, user_data_dir);
  base::CommandLine new_cmd_line(cmd_line.GetProgram());
  base::CommandLine::SwitchMap switches = cmd_line.GetSwitches();
  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the other tests.
  switches.erase(base::kGTestOutputFlag);

  // Create a dedicated temporary directory to store the xml result data
  // per run to ensure clean state and make it possible to launch multiple
  // processes in parallel.
  CHECK(base::CreateTemporaryDirInDir(temp_dir, FILE_PATH_LITERAL("results"),
                                      output_file));
  *output_file = output_file->AppendASCII("test_results.xml");

  new_cmd_line.AppendSwitchPath(switches::kTestLauncherOutput, *output_file);

  // Selecting sample tests to enable switches::kEnableTracing.
  if (switches.find(switches::kEnableTracingFraction) != switches.end()) {
    double enable_tracing_fraction = 0;
    if (!base::StringToDouble(switches[switches::kEnableTracingFraction],
                              &enable_tracing_fraction) ||
        enable_tracing_fraction > 1 || enable_tracing_fraction <= 0) {
      LOG(ERROR) << switches::kEnableTracingFraction
                 << " should have range (0,1].";
    } else {
      // Assuming the hash of all tests are uniformly distributed across the
      // domain of the hash result.
      if (base::PersistentHash(test_name) <=
          UINT32_MAX * enable_tracing_fraction) {
        new_cmd_line.AppendSwitch(switches::kEnableTracing);
      }
    }
    switches.erase(switches::kEnableTracingFraction);
  }

  for (base::CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_cmd_line.AppendSwitchNative(iter->first, iter->second);
  }

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
  new_cmd_line.AppendSwitch(switches::kSingleProcessTests);
  return new_cmd_line;
}

std::string WrapperTestLauncherDelegate::GetWrapper() {
  char* browser_wrapper = getenv("BROWSER_WRAPPER");
  return browser_wrapper ? browser_wrapper : std::string();
}

int WrapperTestLauncherDelegate::GetLaunchOptions() {
  return base::TestLauncher::USE_JOB_OBJECTS |
         base::TestLauncher::ALLOW_BREAKAWAY_FROM_JOB;
}

base::TimeDelta WrapperTestLauncherDelegate::GetTimeout() {
  return TestTimeouts::test_launcher_timeout();
}

void WrapperTestLauncherDelegate::OnTestTimedOut(
    const base::CommandLine& command_line) {
  launcher_delegate_->OnTestTimedOut(command_line);
}

void WrapperTestLauncherDelegate::ProcessTestResults(
    std::vector<base::TestResult>& test_results,
    base::TimeDelta elapsed_time) {
  CHECK_EQ(1u, test_results.size());

  test_results.front().elapsed_time = elapsed_time;

  launcher_delegate_->PostRunTest(&test_results.front());
}
// TODO(isamsonov): crbug.com/1004417 remove when windows builders
// stop flaking on MANAUAL_ tests.
bool WrapperTestLauncherDelegate::ShouldRunTest(
    const base::TestIdentifier& test) {
  return run_manual_tests_ ||
         !base::StartsWith(test.test_name, kManualTestPrefix,
                           base::CompareCase::SENSITIVE);
}

void AppendCommandLineSwitches() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Always disable the unsandbox GPU process for DX12 Info collection to avoid
  // interference. This GPU process is launched 120 seconds after chrome starts.
  command_line->AppendSwitch(switches::kDisableGpuProcessForDX12InfoCollection);
}

}  // namespace

class ContentClientCreator {
 public:
  static void Create(ContentMainDelegate* delegate) {
    SetContentClient(delegate->CreateContentClient());
  }
};

std::string TestLauncherDelegate::GetUserDataDirectoryCommandLineSwitch() {
  return std::string();
}

int LaunchTestsInternal(TestLauncherDelegate* launcher_delegate,
                        size_t parallel_jobs,
                        int argc,
                        char** argv) {
  DCHECK(!g_launcher_delegate);
  g_launcher_delegate = launcher_delegate;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_ANDROID)
  // The ContentMainDelegate is set for browser tests on Android by the
  // browser test target and is not created by the |launcher_delegate|.
  ContentMainParams params(GetContentMainDelegateForTesting());
#else
  std::unique_ptr<ContentMainDelegate> content_main_delegate(
      launcher_delegate->CreateContentMainDelegate());
  ContentClientCreator::Create(content_main_delegate.get());
  // Many tests use GURL during setup, so we need to register schemes early in
  // test launching.
  RegisterContentSchemes();
  ContentMainParams params(content_main_delegate.get());
#endif

#if BUILDFLAG(IS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  InitializeSandboxInfo(&sandbox_info);

  params.instance = GetModuleHandle(NULL);
  params.sandbox_info = &sandbox_info;
#elif BUILDFLAG(IS_MAC)
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(
          command_line->GetProgram().value().c_str(), argc,
          const_cast<const char**>(argv));
  if (seatbelt.sandbox_required) {
    CHECK(seatbelt.server->InitializeSandbox());
  }
#elif !BUILDFLAG(IS_ANDROID)
  params.argc = argc;
  params.argv = const_cast<const char**>(argv);
#endif  // BUILDFLAG(IS_WIN)

  // Disable system tracing for browser tests by default. This prevents breakage
  // of tests that spin the run loop until idle on platforms with system tracing
  // (e.g. Chrome OS). Browser tests exercising this feature re-enable it with a
  // custom system tracing service.
  tracing::PerfettoTracedProcess::SetSystemProducerEnabledForTesting(false);

#if !BUILDFLAG(IS_ANDROID)
  // This needs to be before trying to run tests as otherwise utility processes
  // end up being launched as a test, which leads to rerunning the test.
  // ContentMain is not run on Android in the test process, and is run via
  // java for child processes.
  if (command_line->HasSwitch(switches::kProcessType) ||
      command_line->HasSwitch(switches::kLaunchAsBrowser)) {
    // The main test process has this initialized by the base::TestSuite. But
    // child processes don't have a TestSuite, and must initialize this
    // explicitly before ContentMain.
    TestTimeouts::Initialize();
    return ContentMain(std::move(params));
  }
#endif

  if (command_line->HasSwitch(switches::kSingleProcessTests) ||
      (command_line->HasSwitch(switches::kSingleProcess) &&
       command_line->HasSwitch(base::kGTestFilterFlag)) ||
      command_line->HasSwitch(base::kGTestListTestsFlag) ||
      command_line->HasSwitch(base::kGTestHelpFlag)) {
    g_params = &params;
#if !BUILDFLAG(IS_ANDROID)
    // The call to RunTestSuite() below bypasses TestLauncher, which creates
    // a temporary directory that is used as the user-data-dir. Create a
    // temporary directory now so that the test doesn't use the users home
    // directory as it's data dir.
    base::ScopedTempDir tmp_dir;
    const std::string user_data_dir_switch =
        launcher_delegate->GetUserDataDirectoryCommandLineSwitch();
    if (!user_data_dir_switch.empty() &&
        !command_line->HasSwitch(user_data_dir_switch)) {
      CHECK(tmp_dir.CreateUniqueTempDir());
      command_line->AppendSwitchPath(user_data_dir_switch, tmp_dir.GetPath());
    }
#endif
    return launcher_delegate->RunTestSuite(argc, argv);
  }

  base::AtExitManager at_exit;
  testing::InitGoogleTest(&argc, argv);

  base::TimeTicks start_time(base::TimeTicks::Now());

  // The main test process has this initialized by the base::TestSuite. But
  // this process is just sharding the test off to each main test process, and
  // doesn't have a TestSuite, so must initialize this explicitly as the
  // timeouts are used in the TestLauncher.
  TestTimeouts::Initialize();

  fprintf(stdout,
          "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
          "For debugging a test inside a debugger, use the\n"
          "--gtest_filter=<your_test_name> flag along with either\n"
          "--single-process-tests (to run the test in one launcher/browser "
          "process) or\n"
          "--single-process (to do the above, and also run Chrome in single-"
          "process mode).\n");

  base::debug::VerifyDebugger();

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);
#if BUILDFLAG(IS_POSIX)
  base::FileDescriptorWatcher file_descriptor_watcher(executor.task_runner());
#endif

  launcher_delegate->PreSharding();

  WrapperTestLauncherDelegate delegate(launcher_delegate);
  base::TestLauncher launcher(&delegate, parallel_jobs);
  const int result = launcher.Run() ? 0 : 1;
  launcher_delegate->OnDoneRunningTests();
  fprintf(stdout, "Tests took %" PRId64 " seconds.\n",
          (base::TimeTicks::Now() - start_time).InSeconds());
  fflush(stdout);
  return result;
}

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                size_t parallel_jobs,
                int argc,
                char** argv) {
  base::CommandLine::Init(argc, argv);
  AppendCommandLineSwitches();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // TODO(tluk) Remove deprecation warning after a few releases. Deprecation
  // warning issued version 79.
  if (command_line->HasSwitch("single_process")) {
    fprintf(stderr, "use --single-process-tests instead of --single_process");
    exit(1);
  }

  if (command_line->HasSwitch(switches::kHelpFlag)) {
    PrintUsage();
    return 0;
  }

#if BUILDFLAG(IS_IOS)
  // We need to spawn the UIApplication up for testing, that is done via
  // RunTestsFromIOSApp. We do not want to do this for subprocesses that
  // do not require a UIApplication.
  if (!command_line->HasSwitch(switches::kProcessType) &&
      !command_line->HasSwitch(switches::kLaunchAsBrowser)) {
    base::InitIOSRunHook(base::BindOnce(&LaunchTestsInternal, launcher_delegate,
                                        parallel_jobs, argc, argv));
    return base::RunTestsFromIOSApp();
  }
#endif

  return LaunchTestsInternal(launcher_delegate, parallel_jobs, argc, argv);
}

TestLauncherDelegate* GetCurrentTestLauncherDelegate() {
  return g_launcher_delegate;
}

ContentMainParams CopyContentMainParams() {
  return g_params->ShallowCopyForTesting();
}

bool IsPreTest() {
  auto* test = testing::UnitTest::GetInstance();
  return IsPreTestName(test->current_test_info()->name());
}

}  // namespace content
