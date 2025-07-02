// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/crypto/test/gtest_main.h"

namespace {

class BoringSSLTestSuite : public base::TestSuite {
 public:
  BoringSSLTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

  void Initialize() override {
    TestSuite::Initialize();
    bssl::SetupGoogleTest();
  }
};

}  // namespace

static int InitAndRunAllTests(int argc, char** argv) {
  BoringSSLTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&BoringSSLTestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
