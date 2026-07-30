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

#include <map>
#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/build/configs/buildflags.h"
#include "starboard/extension/native_stability.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/browser/h5vcc_native_stability/native_stability_manager.h"
#endif

#if BUILDFLAG(USE_EVERGREEN)
#include "starboard/extension/crash_handler.h"
#include "starboard/system.h"
#elif BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/browser/cobalt_crash_annotations.h"  // nogncheck
#endif

namespace cobalt {
namespace browser {

#if BUILDFLAG(USE_EVERGREEN)
namespace {

std::map<std::string, std::string>& GetEvergreenTestCrashAnnotations() {
  static base::NoDestructor<std::map<std::string, std::string>> annotations;
  return *annotations;
}

bool TestSetStringCallback(const char* key, const char* value) {
  if (!key || !value) {
    return false;
  }
  GetEvergreenTestCrashAnnotations()[key] = value;
  return true;
}

}  // namespace
#endif  // BUILDFLAG(USE_EVERGREEN)

class HangWatcherDelegateImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_dir_.GetPath(), true, true);
    instance_ = GlobalFeatures::GetInstance();
    delegate_ = std::make_unique<CobaltHangWatcherDelegate>(instance_);
#if BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)
    h5vcc_native_stability::NativeStabilityManager::GetInstance()
        ->SetHangAttributesFilePathForTesting(
            temp_dir_.GetPath().Append("hang_attributes.json"));
#endif
#if BUILDFLAG(USE_EVERGREEN)
    auto* crash_ext = static_cast<const CobaltExtensionCrashHandlerApi*>(
        SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
    if (crash_ext && crash_ext->version >= 3 &&
        crash_ext->RegisterSetStringCallback) {
      crash_ext->RegisterSetStringCallback(&TestSetStringCallback);
    }
#endif
  }

  void TearDown() override {
#if BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)
    task_environment_.RunUntilIdle();
    h5vcc_native_stability::NativeStabilityManager::GetInstance()
        ->ResetForTesting();
#endif
#if BUILDFLAG(USE_EVERGREEN)
    auto* crash_ext = static_cast<const CobaltExtensionCrashHandlerApi*>(
        SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
    if (crash_ext && crash_ext->version >= 3 &&
        crash_ext->RegisterSetStringCallback) {
      crash_ext->RegisterSetStringCallback(nullptr);
    }
    GetEvergreenTestCrashAnnotations().clear();
#endif
    instance_->ClearSetting("EnableHangReporting");
    instance_->ClearSetting("HangWatchTimeSeconds");
    instance_->ClearSetting("HangWatchMonitoringPeriodSeconds");
    instance_->ClearSetting("EnableHangWatchMainThreadDump");
    instance_->ClearSetting("EnableHangWatchIOThreadDump");
    instance_->ClearSetting("EnableHangWatchThreadPoolDump");
    instance_->ClearSetting("EnableHangWatchRendererThreadDump");
    instance_->ClearSetting("EnableHangWatcherLongHangDetection");
    instance_->ClearSetting("EnableHangWatcherLongHangKill");
    instance_->ClearSetting("LongHangTimeoutSeconds");
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_override_;
  GlobalFeatures* instance_ = nullptr;

  std::unique_ptr<CobaltHangWatcherDelegate> delegate_;

#if BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)
  void ClearCrashAnnotations() {
#if BUILDFLAG(USE_EVERGREEN)
    GetEvergreenTestCrashAnnotations().clear();
#elif BUILDFLAG(IS_ANDROIDTV)
    CobaltCrashAnnotations::GetInstance()->ClearAllAnnotations();
#endif
  }

  std::string GetCrashAnnotationValue(const std::string& name) {
#if BUILDFLAG(USE_EVERGREEN)
    const auto& annotations = GetEvergreenTestCrashAnnotations();
    auto it = annotations.find(name);
    return it != annotations.end() ? it->second : "";
#elif BUILDFLAG(IS_ANDROIDTV)
    auto* annotations = CobaltCrashAnnotations::GetInstance();
    auto it = annotations->annotations_.find(name);
    if (it == annotations->annotations_.end()) {
      return "";
    }
    const auto& annotation = *it->second->annotation;
    if (!annotation.is_set()) {
      return "";
    }
    return std::string(annotation.value());
#endif
  }
#endif  // BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)
};

TEST_F(HangWatcherDelegateImplTest, IsHangReportingEnabled_DefaultsWhenUnset) {
  // When unconfigured by H5VCC, defaults to Finch kHangReporting defined in
  // cobalt/browser/features.cc.
  EXPECT_FALSE(delegate_->IsHangReportingEnabled());
}

TEST_F(HangWatcherDelegateImplTest, IsHangReportingEnabled_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(cobalt::features::kHangReporting);

  // Settings not set, should fallback to Finch (true)
  EXPECT_TRUE(delegate_->IsHangReportingEnabled());

  // If we set settings to 0, they should override Finch enabled
  instance_->SetSettings("EnableHangReporting", int64_t(0));
  EXPECT_FALSE(delegate_->IsHangReportingEnabled());
}

TEST_F(HangWatcherDelegateImplTest,
       IsHangReportingEnabled_SettingsOverrideFinchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(cobalt::features::kHangReporting);

  // Settings set to 1 should override Finch disabled
  instance_->SetSettings("EnableHangReporting", int64_t(1));
  EXPECT_TRUE(delegate_->IsHangReportingEnabled());
}

TEST_F(HangWatcherDelegateImplTest, HangWatchTimingFlags_DefaultsWhenUnset) {
  // Defaults when unconfigured:
  // - HangWatchTime: from base::WatchHangsInScope::kDefaultHangWatchTime in
  //   base/threading/hang_watcher.h or kHangWatchTimeSeconds default in
  //   cobalt/browser/features.cc.
  // - HangWatchMonitoringPeriod: from default value of
  //   kHangWatchMonitoringPeriodSeconds in cobalt/browser/features.cc.
  EXPECT_EQ(delegate_->GetHangWatchTime(), base::Seconds(10));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod(), base::Seconds(10));
}

TEST_F(HangWatcherDelegateImplTest, GetHangWatchTime_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["HangWatchTimeSeconds"] = "25";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, params);

  // Settings not set, should fallback to Finch parameter (25s)
  EXPECT_EQ(delegate_->GetHangWatchTime().InSeconds(), 25);

  // If we set settings, they should override Finch
  instance_->SetSettings("HangWatchTimeSeconds", int64_t(15));
  EXPECT_EQ(delegate_->GetHangWatchTime().InSeconds(), 15);
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchTime_NonPositiveSettingsFallbackToDefault) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["HangWatchTimeSeconds"] = "25";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, params);

  // Present but non-positive setting should fallback directly to compiled
  // default (base::WatchHangsInScope::kDefaultHangWatchTime in
  // base/threading/hang_watcher.h), NOT Finch.
  instance_->SetSettings("HangWatchTimeSeconds", int64_t(0));
  EXPECT_EQ(delegate_->GetHangWatchTime().InSeconds(), 10);

  instance_->SetSettings("HangWatchTimeSeconds", int64_t(-5));
  EXPECT_EQ(delegate_->GetHangWatchTime().InSeconds(), 10);
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchMonitoringPeriod_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["HangWatchMonitoringPeriodSeconds"] = "18";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, params);

  // Settings not set, should fallback to Finch parameter (18s)
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod().InSeconds(), 18);

  // If we set settings, they should override Finch
  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(8));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod().InSeconds(), 8);
}

TEST_F(HangWatcherDelegateImplTest, GetHangWatchTime_H5VCCOnly) {
  instance_->SetSettings("HangWatchTimeSeconds", int64_t(30));
  EXPECT_EQ(delegate_->GetHangWatchTime().InSeconds(), 30);
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchTime_InvalidH5VCCFallbackToDefault) {
  // Non-positive H5VCC setting falls back directly to compiled default
  // (base::WatchHangsInScope::kDefaultHangWatchTime in
  // base/threading/hang_watcher.h).
  instance_->SetSettings("HangWatchTimeSeconds", int64_t(0));
  EXPECT_EQ(delegate_->GetHangWatchTime(), base::Seconds(10));

  instance_->SetSettings("HangWatchTimeSeconds", int64_t(-1));
  EXPECT_EQ(delegate_->GetHangWatchTime(), base::Seconds(10));
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchTime_NonPositiveFinchFallbackToDefault) {
  // Non-positive Finch parameter value falls back to compiled default
  // (base::WatchHangsInScope::kDefaultHangWatchTime in
  // base/threading/hang_watcher.h).
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["HangWatchTimeSeconds"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, params);
  EXPECT_EQ(delegate_->GetHangWatchTime(), base::Seconds(10));

  base::test::ScopedFeatureList negative_feature_list;
  base::FieldTrialParams negative_params;
  negative_params["HangWatchTimeSeconds"] = "-5";
  negative_feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, negative_params);
  EXPECT_EQ(delegate_->GetHangWatchTime(), base::Seconds(10));
}

TEST_F(HangWatcherDelegateImplTest, GetHangWatchMonitoringPeriod_H5VCCOnly) {
  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(12));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod().InSeconds(), 12);
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchMonitoringPeriod_NonPositiveSettingsFallbackToDefault) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["HangWatchMonitoringPeriodSeconds"] = "18";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, params);

  // Present but non-positive setting should fallback directly to compiled
  // default (kHangWatchMonitoringPeriodSeconds default in
  // cobalt/browser/features.cc), NOT Finch.
  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(0));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod().InSeconds(), 10);

  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(-5));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod().InSeconds(), 10);
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchMonitoringPeriod_InvalidH5VCCFallbackToDefault) {
  // Non-positive H5VCC setting falls back directly to compiled default
  // (kHangWatchMonitoringPeriodSeconds default in
  // cobalt/browser/features.cc).
  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(0));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod(), base::Seconds(10));

  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", int64_t(-1));
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod(), base::Seconds(10));
}

TEST_F(HangWatcherDelegateImplTest,
       GetHangWatchMonitoringPeriod_NonPositiveFinchFallbackToDefault) {
  // Non-positive Finch parameter value falls back to compiled default
  // (kHangWatchMonitoringPeriodSeconds default in
  // cobalt/browser/features.cc).
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["HangWatchMonitoringPeriodSeconds"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, params);
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod(), base::Seconds(10));

  base::test::ScopedFeatureList negative_feature_list;
  base::FieldTrialParams negative_params;
  negative_params["HangWatchMonitoringPeriodSeconds"] = "-5";
  negative_feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, negative_params);
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod(), base::Seconds(10));
}

TEST_F(HangWatcherDelegateImplTest, IsThreadDumpingEnabled_DefaultsWhenUnset) {
  // Defaults when unconfigured, from feature definitions in
  // cobalt/browser/features.cc:
  // - kHangWatchMainThreadDump
  // - kHangWatchIOThreadDump
  // - kHangWatchThreadPoolDump
  // - kHangWatchRendererThreadDump
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kMainThread));
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kIOThread));
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kThreadPoolThread));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kRendererThread));
}

TEST_F(HangWatcherDelegateImplTest, IsThreadDumpingEnabled_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{cobalt::features::kHangWatchMainThreadDump,
                            cobalt::features::kHangWatchIOThreadDump,
                            cobalt::features::kHangWatchThreadPoolDump},
      /*disabled_features=*/{cobalt::features::kHangWatchRendererThreadDump});

  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kMainThread));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kIOThread));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kThreadPoolThread));
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kRendererThread));
}

TEST_F(HangWatcherDelegateImplTest,
       IsThreadDumpingEnabled_SettingsOverrideFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{cobalt::features::kHangWatchMainThreadDump,
                            cobalt::features::kHangWatchIOThreadDump},
      /*disabled_features=*/{cobalt::features::kHangWatchThreadPoolDump,
                             cobalt::features::kHangWatchRendererThreadDump});

  // Main thread is Finch enabled, but settings set to 0 -> should return false
  instance_->SetSettings("EnableHangWatchMainThreadDump", int64_t(0));
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kMainThread));

  // IO thread is Finch enabled, but settings set to 0 -> should return false
  instance_->SetSettings("EnableHangWatchIOThreadDump", int64_t(0));
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kIOThread));

  // Thread pool is Finch disabled, but settings set to 1 -> should return true
  instance_->SetSettings("EnableHangWatchThreadPoolDump", int64_t(1));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kThreadPoolThread));

  // Renderer thread is Finch disabled, but settings set to 1 -> should return
  // true
  instance_->SetSettings("EnableHangWatchRendererThreadDump", int64_t(1));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kRendererThread));

  // Unrecognized / unsupported thread type should return false
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kCompositorThread));
}

TEST_F(HangWatcherDelegateImplTest, IsThreadDumpingEnabled_H5VCCOnly) {
  instance_->SetSettings("EnableHangWatchMainThreadDump", int64_t(1));
  instance_->SetSettings("EnableHangWatchIOThreadDump", int64_t(1));
  instance_->SetSettings("EnableHangWatchThreadPoolDump", int64_t(1));
  instance_->SetSettings("EnableHangWatchRendererThreadDump", int64_t(0));

  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kMainThread));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kIOThread));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kThreadPoolThread));
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kRendererThread));
}

TEST_F(HangWatcherDelegateImplTest,
       IsLongHangDetectionEnabled_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      cobalt::features::kHangWatcherLongHangDetection);

  // Settings not set, should fallback to Finch (true)
  EXPECT_TRUE(delegate_->IsLongHangDetectionEnabled());

  // If we set settings, they should override Finch
  instance_->SetSettings("EnableHangWatcherLongHangDetection", int64_t(0));
  EXPECT_FALSE(delegate_->IsLongHangDetectionEnabled());
}

TEST_F(HangWatcherDelegateImplTest, IsLongHangKillEnabled_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(cobalt::features::kHangWatcherLongHangKill);

  // Settings not set, should fallback to Finch (true)
  EXPECT_TRUE(delegate_->IsLongHangKillEnabled());

  // If we set settings, they should override Finch
  instance_->SetSettings("EnableHangWatcherLongHangKill", int64_t(0));
  EXPECT_FALSE(delegate_->IsLongHangKillEnabled());
}

TEST_F(HangWatcherDelegateImplTest, GetLongHangTimeout_FallbackToFinch) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LongHangTimeoutSeconds"] = "35";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangWatcherLongHangDetection, params);

  // Settings not set, should fallback to Finch parameter
  EXPECT_EQ(delegate_->GetLongHangTimeout().InSeconds(), 35);

  // If we set settings, they should override Finch
  instance_->SetSettings("LongHangTimeoutSeconds", int64_t(42));
  EXPECT_EQ(delegate_->GetLongHangTimeout().InSeconds(), 42);
}

TEST_F(HangWatcherDelegateImplTest, LongHangFlags_DefaultsWhenUnset) {
  // Defaults when unconfigured, from definitions in cobalt/browser/features.cc:
  // - kHangWatcherLongHangDetection
  // - kHangWatcherLongHangKill
  // - kLongHangTimeoutSeconds
  EXPECT_FALSE(delegate_->IsLongHangDetectionEnabled());
  EXPECT_FALSE(delegate_->IsLongHangKillEnabled());
  EXPECT_EQ(delegate_->GetLongHangTimeout(), base::Seconds(20));
}

TEST_F(HangWatcherDelegateImplTest,
       IsLongHangDetectionEnabled_SettingsOverrideFinchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      cobalt::features::kHangWatcherLongHangDetection);

  // GlobalFeatures setting '1' should override Finch disabled state.
  instance_->SetSettings("EnableHangWatcherLongHangDetection", int64_t(1));
  EXPECT_TRUE(delegate_->IsLongHangDetectionEnabled());
}

TEST_F(HangWatcherDelegateImplTest,
       IsLongHangKillEnabled_SettingsOverrideFinchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      cobalt::features::kHangWatcherLongHangKill);

  // GlobalFeatures setting '1' should override Finch disabled state.
  instance_->SetSettings("EnableHangWatcherLongHangKill", int64_t(1));
  EXPECT_TRUE(delegate_->IsLongHangKillEnabled());
}

TEST_F(HangWatcherDelegateImplTest, IsLongHangDetectionEnabled_H5VCCOnly) {
  instance_->SetSettings("EnableHangWatcherLongHangDetection", int64_t(1));
  EXPECT_TRUE(delegate_->IsLongHangDetectionEnabled());

  instance_->SetSettings("EnableHangWatcherLongHangDetection", int64_t(0));
  EXPECT_FALSE(delegate_->IsLongHangDetectionEnabled());
}

TEST_F(HangWatcherDelegateImplTest, IsLongHangKillEnabled_H5VCCOnly) {
  instance_->SetSettings("EnableHangWatcherLongHangKill", int64_t(1));
  EXPECT_TRUE(delegate_->IsLongHangKillEnabled());

  instance_->SetSettings("EnableHangWatcherLongHangKill", int64_t(0));
  EXPECT_FALSE(delegate_->IsLongHangKillEnabled());
}

TEST_F(HangWatcherDelegateImplTest, GetLongHangTimeout_H5VCCOnly) {
  instance_->SetSettings("LongHangTimeoutSeconds", int64_t(45));
  EXPECT_EQ(delegate_->GetLongHangTimeout().InSeconds(), 45);
}

TEST_F(HangWatcherDelegateImplTest,
       GetLongHangTimeout_NonPositiveSettingsFallbackToDefault) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LongHangTimeoutSeconds"] = "35";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangWatcherLongHangDetection, params);

  // Present but non-positive setting should fallback directly to compiled
  // default (kLongHangTimeoutSeconds default in
  // cobalt/browser/features.cc), NOT Finch.
  instance_->SetSettings("LongHangTimeoutSeconds", int64_t(0));
  EXPECT_EQ(delegate_->GetLongHangTimeout().InSeconds(), 20);

  instance_->SetSettings("LongHangTimeoutSeconds", int64_t(-10));
  EXPECT_EQ(delegate_->GetLongHangTimeout().InSeconds(), 20);
}

TEST_F(HangWatcherDelegateImplTest,
       GetLongHangTimeout_InvalidH5VCCFallbackToDefault) {
  // Non-positive H5VCC setting falls back directly to compiled default
  // (kLongHangTimeoutSeconds default in cobalt/browser/features.cc).
  instance_->SetSettings("LongHangTimeoutSeconds", int64_t(0));
  EXPECT_EQ(delegate_->GetLongHangTimeout(), base::Seconds(20));

  instance_->SetSettings("LongHangTimeoutSeconds", int64_t(-5));
  EXPECT_EQ(delegate_->GetLongHangTimeout(), base::Seconds(20));
}

TEST_F(HangWatcherDelegateImplTest,
       GetLongHangTimeout_NonPositiveFinchFallbackToDefault) {
  // Non-positive Finch parameter value falls back to compiled default
  // (kLongHangTimeoutSeconds default in cobalt/browser/features.cc).
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LongHangTimeoutSeconds"] = "0";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangWatcherLongHangDetection, params);
  EXPECT_EQ(delegate_->GetLongHangTimeout(), base::Seconds(20));

  base::test::ScopedFeatureList negative_feature_list;
  base::FieldTrialParams negative_params;
  negative_params["LongHangTimeoutSeconds"] = "-5";
  negative_feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangWatcherLongHangDetection, negative_params);
  EXPECT_EQ(delegate_->GetLongHangTimeout(), base::Seconds(20));
}

TEST_F(HangWatcherDelegateImplTest, SettingsTypeMismatch_FallsBackToFinch) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LongHangTimeoutSeconds"] = "35";
  feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangWatcherLongHangDetection, params);

  base::test::ScopedFeatureList reporting_feature_list;
  base::FieldTrialParams reporting_params;
  reporting_params["HangWatchTimeSeconds"] = "22";
  reporting_params["HangWatchMonitoringPeriodSeconds"] = "14";
  reporting_feature_list.InitAndEnableFeatureWithParameters(
      cobalt::features::kHangReporting, reporting_params);

  // Inject string variant instead of int64_t
  instance_->SetSettings("EnableHangWatcherLongHangDetection",
                         std::string("true"));
  instance_->SetSettings("LongHangTimeoutSeconds", std::string("42"));
  instance_->SetSettings("HangWatchTimeSeconds", std::string("99"));
  instance_->SetSettings("HangWatchMonitoringPeriodSeconds", std::string("99"));
  instance_->SetSettings("EnableHangWatchMainThreadDump", std::string("1"));
  instance_->SetSettings("EnableHangWatchRendererThreadDump", std::string("0"));

  // Should ignore strings and fallback to Finch
  EXPECT_TRUE(delegate_->IsLongHangDetectionEnabled());
  EXPECT_EQ(delegate_->GetLongHangTimeout().InSeconds(), 35);
  EXPECT_EQ(delegate_->GetHangWatchTime().InSeconds(), 22);
  EXPECT_EQ(delegate_->GetHangWatchMonitoringPeriod().InSeconds(), 14);
  EXPECT_FALSE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kMainThread));
  EXPECT_TRUE(delegate_->IsThreadDumpingEnabled(
      base::HangWatcher::ThreadType::kRendererThread));
}

#if BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)
TEST_F(HangWatcherDelegateImplTest,
       RecordHangStartedAndRecovered_AnnotationUpdated) {
  ClearCrashAnnotations();

  const std::string kHangUuid = "12345678-1234-4234-8234-123456789abc";
  delegate_->RecordHangStarted(kHangUuid);
  EXPECT_EQ(GetCrashAnnotationValue(kNativeStabilityHangUuidKey), kHangUuid);

  delegate_->RecordHangRecovered(kHangUuid);
  EXPECT_EQ(GetCrashAnnotationValue(kNativeStabilityHangUuidKey), "");
}
#endif  // BUILDFLAG(USE_EVERGREEN) || BUILDFLAG(IS_ANDROIDTV)

}  // namespace browser
}  // namespace cobalt
