// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/debug_on_start.h"
#include "base/debug_util.h"
#include "base/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/multiprocess_test.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

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

// Don't add additional code to this method.  Instead add it to
// Initialize().  See bug 6436.
int TestSuite::Run() {
  base::ScopedNSAutoreleasePool scoped_pool;

  Initialize();
  std::string client_func =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kRunClientProcess);
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
