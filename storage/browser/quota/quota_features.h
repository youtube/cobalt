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
// Defers creating default QuotaManager SQLite buckets on CacheStorage binding
// until CacheStorage::Open() is called or persisted CacheStorage data already
// exists on disk for the origin, avoiding cold-start SQLite work on empty
// read/delete probes.
COMPONENT_EXPORT(STORAGE_BROWSER)
BASE_DECLARE_FEATURE(kCobaltDeferCacheStorageQuotaBucket);
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace features

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_FEATURES_H_
