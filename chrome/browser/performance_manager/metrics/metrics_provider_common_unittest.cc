// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider_common.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node.h"

class ScopedAXModeSetter {
 public:
  explicit ScopedAXModeSetter(ui::AXMode new_mode) {
    previous_mode_ = ui::AXPlatformNode::GetAccessibilityMode();
    ui::AXPlatformNode::SetAXMode(new_mode);
  }
  ~ScopedAXModeSetter() {
    ui::AXPlatformNode::SetAXMode(previous_mode_.flags());
  }

 private:
  ui::AXMode previous_mode_;
};

class PerformanceManagerMetricsProviderCommonTest : public testing::Test {
 protected:
  performance_manager::MetricsProviderCommon* provider() {
    return provider_.get();
  }

 private:
  void SetUp() override {
    provider_ = std::make_unique<performance_manager::MetricsProviderCommon>();
  }

  std::unique_ptr<performance_manager::MetricsProviderCommon> provider_;
};

TEST_F(PerformanceManagerMetricsProviderCommonTest, A11yModeOff) {
  base::HistogramTester tester;
  provider()->ProvideCurrentSessionData(nullptr);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", false, 1);
}

TEST_F(PerformanceManagerMetricsProviderCommonTest, A11yModeOn) {
  ScopedAXModeSetter scoped_setter(ui::AXMode::kWebContents);

  base::HistogramTester tester;
  provider()->ProvideCurrentSessionData(nullptr);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", true, 1);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.AccessibilityModeFlag",
      ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS, 1);
}

TEST_F(PerformanceManagerMetricsProviderCommonTest, MultipleA11yModeFlags) {
  ScopedAXModeSetter scoped_setter(ui::AXMode::kWebContents |
                                   ui::AXMode::kHTML);

  base::HistogramTester tester;
  provider()->ProvideCurrentSessionData(nullptr);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", true, 1);
  // Each mode flag gets recorded in its own bucket
  tester.ExpectBucketCount(
      "PerformanceManager.Experimental.AccessibilityModeFlag",
      ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS, 1);
  tester.ExpectBucketCount(
      "PerformanceManager.Experimental.AccessibilityModeFlag",
      ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML, 1);
}
