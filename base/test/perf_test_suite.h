// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_PERF_TEST_SUITE_H_
#define BASE_TEST_PERF_TEST_SUITE_H_
#pragma once

#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

class PerfTestSuite : public TestSuite {
 public:
  PerfTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

  virtual void Initialize() {
    TestSuite::Initialize();

    // Initialize the perf timer log
    FilePath log_path =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath("log-file");
    if (log_path.empty()) {
      FilePath exe;
      PathService::Get(base::FILE_EXE, &exe);
      log_path = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
      log_path = log_path.InsertBeforeExtension(FILE_PATH_LITERAL("_perf"));
    }
    ASSERT_TRUE(InitPerfLog(log_path));

    // Raise to high priority to have more precise measurements. Since we don't
    // aim at 1% precision, it is not necessary to run at realtime level.
    if (!DebugUtil::BeingDebugged())
      base::RaiseProcessToHighPriority();
  }

  virtual void Shutdown() {
    TestSuite::Shutdown();

    FinalizePerfLog();
  }
};

#endif  // BASE_TEST_PERF_TEST_SUITE_H_
