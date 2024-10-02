// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_feature.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace {
// This flag significantly shortens the delay between WebContentsObserver events
// and SnapshotController's StartSnapshot calls. The purpose is to speed up
// integration tests.
const char kOfflinePagesUseTestingSnapshotDelay[] =
    "short-offline-page-snapshot-delay-for-test";
}  // namespace

namespace offline_pages {

BASE_FEATURE(kOfflinePagesCTFeature,
             "OfflinePagesCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOfflinePagesLivePageSharingFeature,
             "OfflinePagesLivePageSharing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOfflinePagesDescriptiveFailStatusFeature,
             "OfflinePagesDescriptiveFailStatus",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOfflinePagesDescriptivePendingStatusFeature,
             "OfflinePagesDescriptivePendingStatus",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOfflinePagesInDownloadHomeOpenInCctFeature,
             "OfflinePagesInDownloadHomeOpenInCct",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOfflinePagesNetworkStateLikelyUnknown,
             "OfflinePagesNetworkStateLikelyUnknown",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPrefetchingOfflinePagesExperimentsOption[] = "exp";

bool IsOfflinePagesCTEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesCTFeature);
}

bool IsOfflinePagesLivePageSharingEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesLivePageSharingFeature);
}

bool ShouldUseTestingSnapshotDelay() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  return cl->HasSwitch(kOfflinePagesUseTestingSnapshotDelay);
}

bool IsOfflinePagesDescriptiveFailStatusEnabled() {
  return base::FeatureList::IsEnabled(
      kOfflinePagesDescriptiveFailStatusFeature);
}

bool IsOfflinePagesDescriptivePendingStatusEnabled() {
  return base::FeatureList::IsEnabled(
      kOfflinePagesDescriptivePendingStatusFeature);
}

bool ShouldOfflinePagesInDownloadHomeOpenInCct() {
  return base::FeatureList::IsEnabled(
      kOfflinePagesInDownloadHomeOpenInCctFeature);
}

bool IsOnTheFlyMhtmlHashComputationEnabled() {
  return false;
}

bool IsOfflinePagesNetworkStateLikelyUnknown() {
  return base::FeatureList::IsEnabled(kOfflinePagesNetworkStateLikelyUnknown);
}

}  // namespace offline_pages
