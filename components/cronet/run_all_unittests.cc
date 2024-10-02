// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
#if defined(CRONET_TESTS_IMPLEMENTATION)
  // cronet_tests[_android] link the Cronet implementation into the test
  // suite statically in many configurations, causing globals initialized by
  // the library (e.g. ThreadPool) to be visible to the TestSuite.
  test_suite.DisableCheckForLeakedGlobals();
#endif
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
