// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

// Note: On Linux, this test currently only fully works on Debug builds.
// See comments in the #ifdef soup if you intend to change this.
#if defined(OS_WIN)
// Always fails on Windows: crbug.com/32070
#define MAYBE_OutputToStream FAILS_OutputToStream
#else
#define MAYBE_OutputToStream OutputToStream
#endif
TEST(StackTrace, MAYBE_OutputToStream) {
  StackTrace trace;

  // Dump the trace into a string.
  std::ostringstream os;
  trace.OutputToStream(&os);
  std::string backtrace_message = os.str();

#if defined(OS_POSIX) && !defined(OS_MACOSX) && NDEBUG
  // Stack traces require an extra data table that bloats our binaries,
  // so they're turned off for release builds.  We stop the test here,
  // at least letting us verify that the calls don't crash.
  return;
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && NDEBUG

  size_t frames_found = 0;
  trace.Addresses(&frames_found);
  ASSERT_GE(frames_found, 5u) <<
      "No stack frames found.  Skipping rest of test.";

  // Check if the output has symbol initialization warning.  If it does, fail.
  ASSERT_EQ(backtrace_message.find("Dumping unresolved backtrace"),
            std::string::npos) <<
      "Unable to resolve symbols.  Skipping rest of test.";

#if defined(OS_MACOSX)
#if 0
  // Disabled due to -fvisibility=hidden in build config.

  // Symbol resolution via the backtrace_symbol function does not work well
  // in OS X.
  // See this thread:
  //
  //    http://lists.apple.com/archives/darwin-dev/2009/Mar/msg00111.html
  //
  // Just check instead that we find our way back to the "start" symbol
  // which should be the first symbol in the trace.
  //
  // TODO(port): Find a more reliable way to resolve symbols.

  // Expect to at least find main.
  EXPECT_TRUE(backtrace_message.find("start") != std::string::npos)
      << "Expected to find start in backtrace:\n"
      << backtrace_message;

#endif
#elif defined(__GLIBCXX__)
  // This branch is for gcc-compiled code, but not Mac due to the
  // above #if.
  // Expect a demangled symbol.
  EXPECT_TRUE(backtrace_message.find("testing::Test::Run()") !=
              std::string::npos)
      << "Expected a demangled symbol in backtrace:\n"
      << backtrace_message;

#elif 0
  // This is the fall-through case; it used to cover Windows.
  // But it's disabled because of varying buildbot configs;
  // some lack symbols.

  // Expect to at least find main.
  EXPECT_TRUE(backtrace_message.find("main") != std::string::npos)
      << "Expected to find main in backtrace:\n"
      << backtrace_message;

#if defined(OS_WIN)
// MSVC doesn't allow the use of C99's __func__ within C++, so we fake it with
// MSVC's __FUNCTION__ macro.
#define __func__ __FUNCTION__
#endif

  // Expect to find this function as well.
  // Note: This will fail if not linked with -rdynamic (aka -export_dynamic)
  EXPECT_TRUE(backtrace_message.find(__func__) != std::string::npos)
      << "Expected to find " << __func__ << " in backtrace:\n"
      << backtrace_message;

#endif  // define(OS_MACOSX)
}

// The test is used for manual testing, e.g., to see the raw output.
TEST(StackTrace, DebugOutputToStream) {
  StackTrace trace;
  std::ostringstream os;
  trace.OutputToStream(&os);
  VLOG(1) << os.str();
}

// The test is used for manual testing, e.g., to see the raw output.
TEST(StackTrace, DebugPrintBacktrace) {
  StackTrace().PrintBacktrace();
}

}  // namespace debug
}  // namespace base
