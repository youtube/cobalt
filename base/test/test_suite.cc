// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debug_on_start_win.h"
#include "base/debug/debugger.h"
#include "base/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
#include "base/test/mock_chrome_application_mac.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

namespace {

class MaybeTestDisabler : public testing::EmptyTestEventListener {
 public:
  virtual void OnTestStart(const testing::TestInfo& test_info) {
    ASSERT_FALSE(TestSuite::IsMarkedMaybe(test_info))
        << "Probably the OS #ifdefs don't include all of the necessary "
           "platforms.\nPlease ensure that no tests have the MAYBE_ prefix "
           "after the code is preprocessed.";
  }
};

}  // namespace

const char TestSuite::kStrictFailureHandling[] = "strict_failure_handling";

TestSuite::TestSuite(int argc, char** argv) {
  base::EnableTerminationOnHeapCorruption();
  CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
#if defined(TOOLKIT_USES_GTK)
  g_thread_init(NULL);
  gtk_init_check(&argc, &argv);
#endif  // defined(TOOLKIT_USES_GTK)
  // Don't add additional code to this constructor.  Instead add it to
  // Initialize().  See bug 6436.
}

TestSuite::~TestSuite() {
  CommandLine::Reset();
}

// static
bool TestSuite::IsMarkedFlaky(const testing::TestInfo& test) {
  return strncmp(test.name(), "FLAKY_", 6) == 0;
}

// static
bool TestSuite::IsMarkedFailing(const testing::TestInfo& test) {
  return strncmp(test.name(), "FAILS_", 6) == 0;
}

// static
bool TestSuite::IsMarkedMaybe(const testing::TestInfo& test) {
  return strncmp(test.name(), "MAYBE_", 6) == 0;
}

// static
bool TestSuite::ShouldIgnoreFailure(const testing::TestInfo& test) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(kStrictFailureHandling))
    return false;
  return IsMarkedFlaky(test) || IsMarkedFailing(test);
}

// static
bool TestSuite::NonIgnoredFailures(const testing::TestInfo& test) {
  return test.should_run() && test.result()->Failed() &&
      !ShouldIgnoreFailure(test);
}

int TestSuite::GetTestCount(TestMatch test_match) {
  testing::UnitTest* instance = testing::UnitTest::GetInstance();
  int count = 0;

  for (int i = 0; i < instance->total_test_case_count(); ++i) {
    const testing::TestCase& test_case = *instance->GetTestCase(i);
    for (int j = 0; j < test_case.total_test_count(); ++j) {
      if (test_match(*test_case.GetTestInfo(j))) {
        count++;
      }
    }
  }

  return count;
}

void TestSuite::CatchMaybeTests() {
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new MaybeTestDisabler);
}

// Don't add additional code to this method.  Instead add it to
// Initialize().  See bug 6436.
int TestSuite::Run() {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  Initialize();
  std::string client_func =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestChildProcess);
  // Check to see if we are being run as a client process.
  if (!client_func.empty())
    return multi_process_function_list::InvokeChildProcessTest(client_func);
  int result = RUN_ALL_TESTS();

  // If there are failed tests, see if we should ignore the failures.
  if (result != 0 && GetTestCount(&TestSuite::NonIgnoredFailures) == 0)
    result = 0;

  // Display the number of flaky tests.
  int flaky_count = GetTestCount(&TestSuite::IsMarkedFlaky);
  if (flaky_count) {
    printf("  YOU HAVE %d FLAKY %s\n\n", flaky_count,
           flaky_count == 1 ? "TEST" : "TESTS");
  }

  // Display the number of tests with ignored failures (FAILS).
  int failing_count = GetTestCount(&TestSuite::IsMarkedFailing);
  if (failing_count) {
    printf("  YOU HAVE %d %s with ignored failures (FAILS prefix)\n\n",
           failing_count, failing_count == 1 ? "test" : "tests");
  }

  // This MUST happen before Shutdown() since Shutdown() tears down
  // objects (such as NotificationService::current()) that Cocoa
  // objects use to remove themselves as observers.
  scoped_pool.Recycle();

  Shutdown();

  return result;
}

// static
void TestSuite::UnitTestAssertHandler(const std::string& str) {
  RAW_LOG(FATAL, str.c_str());
}

void TestSuite::SuppressErrorDialogs() {
#if defined(OS_WIN)
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at
  // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
#endif  // defined(OS_WIN)
}

void TestSuite::Initialize() {
#if defined(OS_MACOSX)
  // Some of the app unit tests spin runloops.
  mock_cr_app::RegisterMockCrApp();
#endif

  // Initialize logging.
  FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
  logging::InitLogging(
      log_filename.value().c_str(),
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  // We want process and thread IDs because we may have multiple processes.
  // Note: temporarily enabled timestamps in an effort to catch bug 6361.
  logging::SetLogItems(true, true, true, true);

  CHECK(base::EnableInProcessStackDumping());
#if defined(OS_WIN)
  // Make sure we run with high resolution timer to minimize differences
  // between production code and test code.
  base::Time::EnableHighResolutionTimer(true);
#endif  // defined(OS_WIN)

  // In some cases, we do not want to see standard error dialogs.
  if (!base::debug::BeingDebugged() &&
      !CommandLine::ForCurrentProcess()->HasSwitch("show-error-dialogs")) {
    SuppressErrorDialogs();
    base::debug::SetSuppressDebugUI(true);
    logging::SetLogAssertHandler(UnitTestAssertHandler);
  }

  icu_util::Initialize();

#if defined(USE_NSS)
  // Trying to repeatedly initialize and cleanup NSS and NSPR may result in
  // a deadlock. Such repeated initialization will happen when using test
  // isolation. Prevent problems by initializing NSS here, so that the cleanup
  // will be done only on process exit.
  base::EnsureNSSInit();
#endif  // defined(USE_NSS)

  CatchMaybeTests();

  TestTimeouts::Initialize();
}

void TestSuite::Shutdown() {
}
