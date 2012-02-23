// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/test_suite.h"
#include "media/base/media.h"

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv, false) {}
  virtual ~TestSuiteNoAtExit() {}
};

int main(int argc, char** argv) {
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

  media::InitializeMediaLibraryForTesting();

  return TestSuiteNoAtExit(argc, argv).Run();
}
