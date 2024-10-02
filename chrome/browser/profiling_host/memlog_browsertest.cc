// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "components/heap_profiling/multi_process/test_driver.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

// Some builds don't support memlog in which case the tests won't function.
#if BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace heap_profiling {

struct TestParam {
  Mode mode;
  mojom::StackMode stack_mode;
  bool start_profiling_with_command_line_flag;
};

class MemlogBrowserTest : public PlatformBrowserTest,
                          public testing::WithParamInterface<TestParam> {
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);

    if (!GetParam().start_profiling_with_command_line_flag)
      return;

    if (GetParam().mode == Mode::kBrowser) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogMode,
                                      heap_profiling::kMemlogModeBrowser);
    }

    if (GetParam().stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES) {
      command_line->AppendSwitchASCII(
          heap_profiling::kMemlogStackMode,
          heap_profiling::kMemlogStackModeNativeWithThreadNames);
    } else {
      NOTREACHED();
    }

    // Use a sampling rate of 10k.
    command_line->AppendSwitchASCII(heap_profiling::kMemlogSamplingRate,
                                    heap_profiling::kMemlogSamplingRate10KB);
  }
};

// TODO(crbug.com/1223739) Disabled due to flakiness.
// Ensure invocations via TracingController can generate a valid JSON file with
// expected data.
IN_PROC_BROWSER_TEST_P(MemlogBrowserTest, DISABLED_EndToEnd) {
  LOG(INFO) << "Memlog mode: " << static_cast<int>(GetParam().mode);
  LOG(INFO) << "Memlog stack mode: " << static_cast<int>(GetParam().stack_mode);
  LOG(INFO) << "Started via command line flag: "
            << GetParam().start_profiling_with_command_line_flag;
  TestDriver driver;
  TestDriver::Options options;
  options.mode = GetParam().mode;
  options.stack_mode = GetParam().stack_mode;
  options.profiling_already_started =
      GetParam().start_profiling_with_command_line_flag;

  EXPECT_TRUE(driver.RunTest(options));
}

// Memlog tests are expensive, so we choose configurations that make the most
// sense to test.
std::vector<TestParam> GetParams() {
  std::vector<TestParam> params;

  // Test that if we don't start profiling, nothing happens.
  params.push_back({Mode::kNone, mojom::StackMode::NATIVE_WITH_THREAD_NAMES,
                    false /* start_profiling_with_command_line_flag */});

  // Test that we can start profiling with command line flag.
  params.push_back({Mode::kBrowser, mojom::StackMode::NATIVE_WITH_THREAD_NAMES,
                    true /* start_profiling_with_command_line_flag */});

  // Test that we can start profiling without command line flag.
  params.push_back({Mode::kBrowser, mojom::StackMode::NATIVE_WITH_THREAD_NAMES,
                    false /* start_profiling_with_command_line_flag */});

  // Test that we can start profiling for the renderer process.
  params.push_back({Mode::kAllRenderers,
                    mojom::StackMode::NATIVE_WITH_THREAD_NAMES,
                    false /* start_profiling_with_command_line_flag */});

  return params;
}

INSTANTIATE_TEST_SUITE_P(Memlog,
                         MemlogBrowserTest,
                         ::testing::ValuesIn(GetParams()));

}  // namespace heap_profiling

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
