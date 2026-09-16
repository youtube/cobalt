// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)

class JavaStartupMetricsBrowserTestBase : public content::ContentBrowserTest {
 public:
  JavaStartupMetricsBrowserTestBase() = default;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    if (base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_dir_)) {
      prev_state_file_ =
          app_data_dir_.AppendASCII("java_startup_state_previous.bin");
      WriteTestData();
    }
    content::ContentBrowserTest::SetUp();
  }

  // To be overridden by subclasses
  virtual void WriteTestData() {}

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::FilePath app_data_dir_;
  base::FilePath prev_state_file_;
};

// 1. Valid Data Test
class JavaStartupMetricsValidTest : public JavaStartupMetricsBrowserTestBase {
  void WriteTestData() override {
    uint64_t fake_status = (1ULL << 1) | (1ULL << 3);
    std::string data(reinterpret_cast<const char*>(&fake_status), 8);
    base::WriteFile(prev_state_file_, data);
  }
};

IN_PROC_BROWSER_TEST_F(JavaStartupMetricsValidTest,
                       HarvestsPreviousSessionMetrics) {
  histogram_tester_->ExpectBucketCount("Cobalt.Startup.MilestoneReached", 1, 1);
  histogram_tester_->ExpectBucketCount("Cobalt.Startup.MilestoneReached", 3, 1);
  histogram_tester_->ExpectBucketCount("Cobalt.Startup.MilestoneReached", 2, 0);

  EXPECT_FALSE(base::PathExists(prev_state_file_));
}

// 2. Missing Data Test
class JavaStartupMetricsMissingTest : public JavaStartupMetricsBrowserTestBase {
  void WriteTestData() override { base::DeleteFile(prev_state_file_); }
};

IN_PROC_BROWSER_TEST_F(JavaStartupMetricsMissingTest,
                       HandlesMissingFileQuietly) {
  histogram_tester_->ExpectTotalCount("Cobalt.Startup.MilestoneReached", 0);
  EXPECT_FALSE(base::PathExists(prev_state_file_));
}

// 3. Corrupted Data Test
class JavaStartupMetricsCorruptTest : public JavaStartupMetricsBrowserTestBase {
  void WriteTestData() override {
    // Write only 4 bytes instead of the required 8
    std::string data = "1234";
    base::WriteFile(prev_state_file_, data);
  }
};

IN_PROC_BROWSER_TEST_F(JavaStartupMetricsCorruptTest,
                       DeletesCorruptFileSafely) {
  histogram_tester_->ExpectTotalCount("Cobalt.Startup.MilestoneReached", 0);
  // It should STILL delete the corrupted file so it doesn't block future tests
  EXPECT_FALSE(base::PathExists(prev_state_file_));
}

#endif  // BUILDFLAG(IS_ANDROID)
