// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace storage {

namespace features {

COMPONENT_EXPORT(STORAGE_BROWSER) BASE_DECLARE_FEATURE(kStorageQuotaSettings);
extern const base::FeatureParam<double> kMustRemainAvailableBytes;
extern const base::FeatureParam<double> kMustRemainAvailableRatio;
extern const base::FeatureParam<double> kPoolSizeBytes;
extern const base::FeatureParam<double> kPoolSizeRatio;
extern const base::FeatureParam<double> kShouldRemainAvailableBytes;
extern const base::FeatureParam<double> kShouldRemainAvailableRatio;

COMPONENT_EXPORT(STORAGE_BROWSER) BASE_DECLARE_FEATURE(kStaticStorageQuota);

#if BUILDFLAG(IS_COBALT)
// Skips cold-start ServiceWorker LevelDB directory scans/opens when no
// registration database exists on disk (reclaiming leftover empty databases),
// avoids creating default QuotaManager SQLite buckets on read-only/delete
// CacheStorage probes when no caches exist, and defers non-critical
// QuotaManager cleanup sweeps past startup.
COMPONENT_EXPORT(STORAGE_BROWSER)
BASE_DECLARE_FEATURE(kCobaltLazyServiceWorkerAndQuotaInit);
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace features

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_
