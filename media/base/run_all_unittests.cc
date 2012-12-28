// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/test_suite.h"
#include "media/base/media.h"

#if defined(__LB_SHELL__)
#include "lb_shell_platform_delegate.h"
#include "lb_stack.h"
#endif

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv, false) {}
  virtual ~TestSuiteNoAtExit() {}
};

int main(int argc, char** argv) {
#if defined(__LB_SHELL__)
  LBShellPlatformDelegate::PlatformInit();
  LB::SetStackSize();
#endif

  // By default command-line parsing happens only in TestSuite::Run(), but
  // that's too late to get VLOGs and so on from
  // InitializeMediaLibraryForTesting() below.  Instead initialize logging
  // explicitly here (and have it get re-initialized by TestSuite::Run()).
  CommandLine::Init(argc, argv);
  CHECK(logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::DONT_LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE,
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS));
  base::AtExitManager exit_manager;

#if !defined(__LB_SHELL__)
  // link error: platform specific implementation, not implemented in shell
  media::InitializeMediaLibraryForTesting();
#endif

  return TestSuiteNoAtExit(argc, argv).Run();
}
