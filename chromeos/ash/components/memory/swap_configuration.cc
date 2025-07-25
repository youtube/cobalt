// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/swap_configuration.h"

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"

namespace ash {

// There are going to be 2 separate experiments for memory pressure signal, one
// for ARC enabled users and one for ARC disabled users.
BASE_FEATURE(kCrOSMemoryPressureSignalStudyNonArc,
             "ChromeOSMemoryPressureSignalStudyNonArc",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyNonArcCriticalBps{
    &kCrOSMemoryPressureSignalStudyNonArc, "critical_threshold_percentage",
    520};

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyNonArcModerateBps{
    &kCrOSMemoryPressureSignalStudyNonArc, "moderate_threshold_percentage",
    4000};

BASE_FEATURE(kCrOSMemoryPressureSignalStudyArc,
             "ChromeOSMemoryPressureSignalStudyArc",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyArcCriticalBps{
    &kCrOSMemoryPressureSignalStudyArc, "critical_threshold_percentage", 800};

const base::FeatureParam<int> kCrOSMemoryPressureSignalStudyArcModerateBps{
    &kCrOSMemoryPressureSignalStudyArc, "moderate_threshold_percentage", 4000};

BASE_FEATURE(kCrOSEnableZramWriteback,
             "ChromeOSZramWriteback",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kCrOSWritebackPeriodicTimeSec{
    &kCrOSEnableZramWriteback, "ZramWritebackPeriodicTimeSec",
    base::Seconds(10).InSeconds()};
const base::FeatureParam<int> kCrOSZramWritebackWritebackBackoffTimeSec{
    &kCrOSEnableZramWriteback, "CrOSZramWritebackWritebackBackoffTimeSec",
    base::Seconds(30).InSeconds()};
const base::FeatureParam<int> kCrOSZramWritebackDevSizePctFree{
    &kCrOSEnableZramWriteback, "ZramBDSizePctOfFree", 15};
const base::FeatureParam<int> kCrOSWritebackMinPages{
    &kCrOSEnableZramWriteback, "ZramWritebackMinPages",
    ((2 << 20) / 4096) /* 2MiB worth of pages */};
const base::FeatureParam<int> kCrOSWritebackMaxPages{
    &kCrOSEnableZramWriteback, "ZramWritebackMaxPages",
    ((128 << 20) / 4096) /* 128MiB worth of pages */
};
const base::FeatureParam<bool> kCrOSWritebackHuge{&kCrOSEnableZramWriteback,
                                                  "ZramWritebackHuge", false};
const base::FeatureParam<bool> kCrOSWritebackHugeIdle{
    &kCrOSEnableZramWriteback, "ZramWritebackHugeIdle", true};
const base::FeatureParam<bool> kCrOSWritebackIdle{&kCrOSEnableZramWriteback,
                                                  "ZramWritebackIdle", true};
const base::FeatureParam<int> kCrOSWritebackIdleMinTimeSec{
    &kCrOSEnableZramWriteback, "ZramWritebackIdleMinTimeSec",
    base::Minutes(2).InSeconds()};
const base::FeatureParam<int> kCrOSWritebackIdleMaxTimeSec{
    &kCrOSEnableZramWriteback, "ZramWritebackIdleMaxTimeSec",
    base::Days(1).InSeconds()};

const ZramWritebackParams ZramWritebackParams::Get() {
  ZramWritebackParams params;
  params.periodic_time = base::Seconds(kCrOSWritebackPeriodicTimeSec.Get());
  params.backoff_time =
      base::Seconds(kCrOSZramWritebackWritebackBackoffTimeSec.Get());
  params.idle_max_time = base::Seconds(kCrOSWritebackIdleMaxTimeSec.Get());
  params.idle_min_time = base::Seconds(kCrOSWritebackIdleMinTimeSec.Get());
  params.max_pages = kCrOSWritebackMaxPages.Get();
  params.min_pages = kCrOSWritebackMinPages.Get();
  params.writeback_huge_idle = kCrOSWritebackHugeIdle.Get();
  params.writeback_huge = kCrOSWritebackHuge.Get();
  params.writeback_idle = kCrOSWritebackIdle.Get();
  return params;
}

namespace {

void OnMemoryMarginsSet(bool result, uint64_t critical, uint64_t moderate) {
  if (!result) {
    LOG(ERROR) << "Unable to set critical memory margins via resourced";
    return;
  }

  LOG(WARNING) << "Set memory margins via resourced to: " << critical
               << "KB and " << moderate << "KB";
}

void ConfigureResourcedPressureThreshold(bool arc_enabled) {
  if (!ResourcedClient::Get()) {
    return;
  }

  bool experiment_enabled = false;
  int critical_bps = 0;
  int moderate_bps = 0;
  if (arc_enabled) {
    experiment_enabled =
        base::FeatureList::IsEnabled(kCrOSMemoryPressureSignalStudyArc);
    if (experiment_enabled) {
      critical_bps = kCrOSMemoryPressureSignalStudyArcCriticalBps.Get();
      moderate_bps = kCrOSMemoryPressureSignalStudyArcModerateBps.Get();
    }
  } else {
    experiment_enabled =
        base::FeatureList::IsEnabled(kCrOSMemoryPressureSignalStudyNonArc);
    if (experiment_enabled) {
      critical_bps = kCrOSMemoryPressureSignalStudyNonArcCriticalBps.Get();
      moderate_bps = kCrOSMemoryPressureSignalStudyNonArcModerateBps.Get();
    }
  }

  if (experiment_enabled) {
    // We need to send a debus message to resourced with the critical threshold
    // value (in bps).
    if (critical_bps < 520 || critical_bps > 2500 || moderate_bps > 7500 ||
        moderate_bps < 2000 || critical_bps >= moderate_bps) {
      // To avoid a potentially catastrophic misconfiguration we
      // only allow critical values between 5.2% and 25%, moderate between 20%
      // and 75%, and moderate must be greater than critical.
      LOG(ERROR) << "Invalid values specified for memory thresholds: "
                 << critical_bps << " and " << moderate_bps;
      return;
    }

    LOG(WARNING) << "Overriding memory thresholds with values "
                 << (critical_bps / 100.0) << "% and " << (moderate_bps / 100.0)
                 << "%";
    ResourcedClient::Get()->SetMemoryMarginsBps(
        critical_bps, moderate_bps, base::BindOnce(&OnMemoryMarginsSet));
  }
}

}  // namespace

void ConfigureSwap(bool arc_enabled) {
  ConfigureResourcedPressureThreshold(arc_enabled);
}

}  // namespace ash
