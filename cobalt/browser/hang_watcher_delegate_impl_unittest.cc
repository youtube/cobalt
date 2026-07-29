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

#include "cobalt/browser/hang_watcher_delegate_impl.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cobalt/browser/global_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

class HangWatcherDelegateImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_dir_.GetPath(), true, true);
    instance_ = GlobalFeatures::GetInstance();
    delegate_ = std::make_unique<CobaltHangWatcherDelegate>(instance_);
  }

  void TearDown() override {
    instance_->ClearSetting("EnableHangReporting");
    instance_->ClearSetting("HangWatchTimeSeconds");
    instance_->ClearSetting("HangWatchMonitoringPeriodSeconds");
    instance_->ClearSetting("EnableHangWatchMainThreadDump");
    instance_->ClearSetting("EnableHangWatchIOThreadDump");
    instance_->ClearSetting("EnableHangWatchThreadPoolDump");
    instance_->ClearSetting("EnableHangWatchRendererThreadDump");
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_override_;
  GlobalFeatures* instance_ = nullptr;

  std::unique_ptr<CobaltHangWatcherDelegate> delegate_;
};

TEST_F(HangWatcherDelegateImplTest, IsHangReportingEnabled_Default) {
  // Should default to base::FeatureList value (which we can't easily mock here,
  // but it shouldn't crash).
  delegate_->IsHangReportingEnabled();
}

TEST_F(HangWatcherDelegateImplTest,
       IsHangReportingEnabledReturnsValueFromGlobalSettings) {
  instance_->SetSettings("EnableHangReporting", int64_t(1));
  EXPECT_TRUE(delegate_->IsHangReportingEnabled());

  instance_->SetSettings("EnableHangReporting", int64_t(0));
  EXPECT_FALSE(delegate_->IsHangReportingEnabled());
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchTimeReturnsValueFromGlobalSettings) {
  instance_->SetSettings("HangWatchTimeSeconds", int64_t(15));

  auto timeout = delegate_->GetHangWatchTime();
  ASSERT_TRUE(timeout.has_value());
  EXPECT_EQ(timeout->InSeconds(), 15);
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchMonitoringPeriodReturnsValueFromGlobalSettings) {
  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(8));
  auto period = delegate_->GetHangWatchMonitoringPeriod();
  ASSERT_TRUE(period.has_value());
  EXPECT_EQ(period->InSeconds(), 8);
}

TEST_F(HangWatcherDelegateImplTest,
       IsThreadDumpingEnabledReturnsValueFromGlobalSettings) {
  instance_->SetSettings("EnableHangWatchIOThreadDump", int64_t(1));
  auto io_enabled = delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kIOThread);
  ASSERT_TRUE(io_enabled.has_value());
  EXPECT_TRUE(*io_enabled);

  instance_->SetSettings("EnableHangWatchIOThreadDump", int64_t(0));
  io_enabled = delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kIOThread);
  ASSERT_TRUE(io_enabled.has_value());
  EXPECT_FALSE(*io_enabled);

  instance_->SetSettings("EnableHangWatchRendererThreadDump", int64_t(1));
  auto renderer_enabled = delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kRendererThread);
  ASSERT_TRUE(renderer_enabled.has_value());
  EXPECT_TRUE(*renderer_enabled);
}

}  // namespace browser
}  // namespace cobalt
