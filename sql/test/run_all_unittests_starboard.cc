// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "sql/test/sql_test_suite.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

static int InitAndRunAllTests(int argc, char** argv) {
  sql::SQLTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&sql::SQLTestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
