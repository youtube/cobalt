// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_SUITE_H_
#define BASE_TEST_SUITE_H_

// Defines a basic test suite framework for running gtest based tests.  You can
// instantiate this class in your main function and call its Run method to run
// any gtest based tests that are linked into your executable.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/debug_on_start.h"
#include "base/i18n/icu_util.h"
#include "base/multiprocess_test.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

// Match function used by the GetTestCount method.
typedef bool (*TestMatch)(const testing::TestInfo&);

class TestSuite {
 public:
  TestSuite(int argc, char** argv) {
    base::EnableTerminationOnHeapCorruption();
    CommandLine::Init(argc, argv);
    testing::InitGoogleTest(&argc, argv);
#if defined(OS_LINUX)
    g_thread_init(NULL);
    gtk_init_check(&argc, &argv);
#endif
    // Don't add additional code to this constructor.  Instead add it to
    // Initialize().  See bug 6436.
  }

  virtual ~TestSuite() {
    CommandLine::Reset();
  }

  // Returns true if a string starts with FLAKY_.
  static bool IsFlaky(const char* name) {
    return strncmp(name, "FLAKY_", 6) == 0;
  }

  // Returns true if the test is marked as flaky.
  static bool FlakyTest(const testing::TestInfo& test) {
    return IsFlaky(test.name()) || IsFlaky(test.test_case_name());
  }

  // Returns true if the test failed and is not marked as flaky.
  static bool NonFlakyFailures(const testing::TestInfo& test) {
    return test.should_run() && test.result()->Failed() && !FlakyTest(test);
  }

  // Returns the number of tests where the match function returns true.
  int GetTestCount(TestMatch test_match) {
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

  // Don't add additional code to this method.  Instead add it to
  // Initialize().  See bug 6436.
  int Run() {
    base::ScopedNSAutoreleasePool scoped_pool;

    Initialize();
    std::wstring client_func =
        CommandLine::ForCurrentProcess()->GetSwitchValue(kRunClientProcess);
    // Check to see if we are being run as a client process.
    if (!client_func.empty()) {
      // Convert our function name to a usable string for GetProcAddress.
      std::string func_name(client_func.begin(), client_func.end());

      return multi_process_function_list::InvokeChildProcessTest(func_name);
    }
    int result = RUN_ALL_TESTS();

    // Reset the result code if only flaky test failed.
    if (result != 0 && GetTestCount(&TestSuite::NonFlakyFailures) == 0) {
      result = 0;
    }

    // Display the number of flaky tests.
    int flaky_count = GetTestCount(&TestSuite::FlakyTest);
    if (flaky_count) {
      printf("  YOU HAVE %d FLAKY %s\n\n", flaky_count,
             flaky_count == 1 ? "TEST" : "TESTS");
    }

    // This MUST happen before Shutdown() since Shutdown() tears down
    // objects (such as NotificationService::current()) that Cocoa
    // objects use to remove themselves as observers.
    scoped_pool.Recycle();

    Shutdown();

    return result;
  }

 protected:
  // All fatal log messages (e.g. DCHECK failures) imply unit test failures.
  static void UnitTestAssertHandler(const std::string& str) {
    FAIL() << str;
  }

#if defined(OS_WIN)
  // Disable crash dialogs so that it doesn't gum up the buildbot
  virtual void SuppressErrorDialogs() {
    UINT new_flags = SEM_FAILCRITICALERRORS |
                     SEM_NOGPFAULTERRORBOX |
                     SEM_NOOPENFILEERRORBOX;

    // Preserve existing error mode, as discussed at
    // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);
  }
#endif

  // Override these for custom initialization and shutdown handling.  Use these
  // instead of putting complex code in your constructor/destructor.

  virtual void Initialize() {
    // Initialize logging.
    FilePath exe;
    PathService::Get(base::FILE_EXE, &exe);
    FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
    logging::InitLogging(log_filename.value().c_str(),
                         logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                         logging::LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);
    // We want process and thread IDs because we may have multiple processes.
    // Note: temporarily enabled timestamps in an effort to catch bug 6361.
    logging::SetLogItems(true, true, true, true);

    CHECK(base::EnableInProcessStackDumping());
#if defined(OS_WIN)
    // For unit tests we turn on the high resolution timer and disable
    // base::Time's use of SystemMonitor. Tests create and destroy the message
    // loop, which causes a crash with SystemMonitor (http://crbug.com/12187).
    base::Time::EnableHiResClockForTests();

    // In some cases, we do not want to see standard error dialogs.
    if (!IsDebuggerPresent() &&
        !CommandLine::ForCurrentProcess()->HasSwitch("show-error-dialogs")) {
      SuppressErrorDialogs();
#if !defined(PURIFY)
      // When the code in this file moved around, bug 6436 resurfaced.
      // As a hack workaround, just #ifdef out this code for Purify builds.
      logging::SetLogAssertHandler(UnitTestAssertHandler);
#endif  // !defined(PURIFY)
    }
#endif  // defined(OS_WIN)

    icu_util::Initialize();
  }

  virtual void Shutdown() {
  }

  // Make sure that we setup an AtExitManager so Singleton objects will be
  // destroyed.
  base::AtExitManager at_exit_manager_;
};

#endif  // BASE_TEST_SUITE_H_
