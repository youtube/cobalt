// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"

#include "base/containers/extend.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif

namespace web_app {

WebAppsSyncTestBase::WebAppsSyncTestBase(TestType test_type)
    : SyncTest(test_type) {
  std::vector<base::test::FeatureRef> disabled_features;

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1357905): Update test driver to work with new UI.
  disabled_features.push_back(apps::features::kLinkCapturingUiUpdate);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Disable Lacros, so that Web Apps get synced in the Ash browser.
  // TODO(crbug.com/1462253): Also test with Lacros flags enabled.
  base::Extend(disabled_features, ash::standalone_browser::GetFeatureRefs());
#endif

  scoped_feature_list_.InitWithFeatures({}, disabled_features);
}

WebAppsSyncTestBase::~WebAppsSyncTestBase() = default;

}  // namespace web_app
