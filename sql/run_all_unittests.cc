// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "sql/test_vfs.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

int TestSuiteRun(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  sql::RegisterTestVfs();
  int error_level = base::TestSuite(argc, argv).Run();
  sql::UnregisterTestVfs();
  return error_level;
}

STARBOARD_WRAP_SIMPLE_MAIN(TestSuiteRun);
