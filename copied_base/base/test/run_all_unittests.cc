// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/com_init_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include "base/test/allow_check_is_test_for_testing.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#endif  // BUILDFLAG(IS_COBALT_HERMETIC_BUILD)

namespace base {

namespace {

#if BUILDFLAG(IS_WIN)
class ComLeakCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test) override {
    // Verify that COM has been reset to defaults by the test.
    EXPECT_EQ(win::GetComApartmentTypeForThread(), win::ComApartmentType::NONE);
  }
};

class HistogramAllocatorCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test) override {
    // Verify that the histogram allocator was released by the test.
    CHECK(!GlobalHistogramAllocator::Get());
  }
};

class TimerCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test_info) override {
    EXPECT_FALSE(Time::IsHighResolutionTimerInUse());
  }
};
#endif  // BUILDFLAG(IS_WIN)

class BaseUnittestSuite : public TestSuite {
 public:
  BaseUnittestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    TestSuite::Initialize();

#if BUILDFLAG(IS_WIN)
    // Add TestEventListeners to enforce certain properties across tests.
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new ComLeakCheck);
    listeners.Append(new HistogramAllocatorCheck);
    listeners.Append(new TimerCheck);
#endif  // BUILDFLAG(IS_WIN)
  }
};

}  // namespace

}  // namespace base

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
static int InitAndRunAllTests(int argc, char** argv) {
  base::test::AllowCheckIsTestForTesting();
  return base::TestSuite(argc, argv).Run();
}

// For the Starboard OS define SbEventHandle as the entry point
SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
// Define main() for non-Evergreen Starboard OS.
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif  // !SB_IS(EVERGREEN)
#else
int main(int argc, char** argv) {
  base::BaseUnittestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
#endif  // BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
