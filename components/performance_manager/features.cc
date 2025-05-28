// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager::features {

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBackgroundTabLoadingFromPerformanceManager,
             "BackgroundTabLoadingFromPerformanceManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kBackgroundTabLoadingMinSiteEngagement,
                   &kBackgroundTabLoadingFromPerformanceManager,
                   "min_site_engagement",
                   0);

BASE_FEATURE_PARAM(bool,
                   kBackgroundTabLoadingRestoreMainFrameState,
                   &kBackgroundTabLoadingFromPerformanceManager,
                   "restore_main_frame_state",
                   true);

BASE_FEATURE(kPerformanceControlsPerformanceSurvey,
             "PerformanceControlsPerformanceSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsBatteryPerformanceSurvey,
             "PerformanceControlsBatteryPerformanceSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The variable was renamed to "MemorySaver" but the experiment name remains as
// "HighEfficiency" because it is already running (crbug.com/1493843).
BASE_FEATURE(kPerformanceControlsMemorySaverOptOutSurvey,
             "PerformanceControlsHighEfficiencyOptOutSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsBatterySaverOptOutSurvey,
             "PerformanceControlsBatterySaverOptOutSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kPerformanceControlsBatterySurveyLookback{
        &kPerformanceControlsBatteryPerformanceSurvey, "battery_lookback",
        base::Days(8)};

BASE_FEATURE(kPerformanceInterventionUI,
             "PerformanceInterventionUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceInterventionDemoMode,
             "PerformanceInterventionDemoMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldUsePerformanceInterventionBackend() {
  return base::FeatureList::IsEnabled(kPerformanceInterventionUI);
}

const base::FeatureParam<int> kInterventionDialogStringVersion{
    &kPerformanceInterventionUI, "intervention_dialog_version", 1};

const base::FeatureParam<bool> kInterventionShowMixedProfileSuggestions{
    &kPerformanceInterventionUI, "intervention_show_mixed_profile", false};

const base::FeatureParam<base::TimeDelta> kInterventionButtonTimeout{
    &kPerformanceInterventionUI, "intervention_button_timeout",
    base::Seconds(10)};

const base::FeatureParam<base::TimeDelta> kCPUTimeOverThreshold{
    &kPerformanceInterventionUI, "cpu_time_over_threshold", base::Seconds(60)};
const base::FeatureParam<base::TimeDelta> kCPUSampleFrequency{
    &kPerformanceInterventionUI, "cpu_sample_frequency", base::Seconds(15)};

const base::FeatureParam<int> kCPUDegradedHealthPercentageThreshold{
    &kPerformanceInterventionUI, "cpu_degraded_percent_threshold", 50};
const base::FeatureParam<int> kCPUUnhealthyPercentageThreshold{
    &kPerformanceInterventionUI, "cpu_unhealthy_percent_threshold", 75};

const base::FeatureParam<int> kCPUMaxActionableTabs{
    &kPerformanceInterventionUI, "cpu_max_actionable_tabs", 4};

const base::FeatureParam<int> kMinimumActionableTabCPUPercentage{
    &kPerformanceInterventionUI, "minimum_actionable_tab_cpu", 10};

const base::FeatureParam<base::TimeDelta> kMemoryTimeOverThreshold{
    &kPerformanceInterventionUI, "memory_time_over_threshold",
    base::Seconds(60)};

const base::FeatureParam<int> kMemoryFreePercentThreshold{
    &kPerformanceInterventionUI, "memory_free_percent_threshold", 10};
const base::FeatureParam<int> kMemoryFreeBytesThreshold{
    &kPerformanceInterventionUI, "memory_free_bytes_threshold",
    1024 * 1024 * 1024};

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUnthrottledTabProcessReporting,
             "UnthrottledTabProcessReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif

BASE_FEATURE(kPMProcessPriorityPolicy,
             "PMProcessPriorityPolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kInheritParentPriority{
    &kPMProcessPriorityPolicy, "inherit_parent_priority", true};

BASE_FEATURE(kPMLoadingPageVoter,
             "PMLoadingPageVoter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBFCachePerformanceManagerPolicy,
             "BFCachePerformanceManagerPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUrgentPageDiscarding,
             "UrgentPageDiscarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCPUMeasurementInFreezingPolicy,
             "CPUMeasurementInFreezingPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryMeasurementInFreezingPolicy,
             "MemoryMeasurementInFreezingPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardFrozenBrowsingInstancesWithGrowingPMF,
             "DiscardFrozenBrowsingInstancesWithGrowingPMF",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Note: These params are associated with `kCPUMeasurementInFreezingPolicy`
// instead of `kFreezingOnBatterySaver` or
// `kDiscardFrozenBrowsingInstancesWithGrowingPMF`, to allow retrieving the
// value without activating these two features.
BASE_FEATURE_PARAM(int,
                   kFreezingMemoryGrowthThresholdToDiscardKb,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_memory_growth_threshold_to_discard_kb",
                   /* 100 MB */ 100 * 1024);

BASE_FEATURE_PARAM(double,
                   kFreezingHighCPUProportion,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_high_cpu_proportion",
                   0.25);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFreezingVisibleProtectionTime,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_visible_protection_time",
                   base::Minutes(5));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFreezingAudioProtectionTime,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_audio_protection_time",
                   base::Minutes(5));

BASE_FEATURE(kFreezingOnBatterySaver,
             "FreezingOnBatterySaver",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFreezingOnBatterySaverForTesting,
             "FreezingOnBatterySaverForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFreezingFollowsDiscardOptOut,
             "FreezingFollowsDiscardOptOut",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRecordFreezingEligibilityUKM,
             "RecordFreezingEligibilityUKM",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResourceAttributionIncludeOrigins,
             "ResourceAttributionIncludeOrigins",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSeamlessRenderFrameSwap,
             "SeamlessRenderFrameSwap",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnimportantFramesPriority,
             "UnimportantFramesPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleUnimportantFrameRate,
             "ThrottleUnimportantFrameRate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeepDefaultSearchEngineRendererAlive,
             "KeepDefaultSearchEngineRendererAlive",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace performance_manager::features
