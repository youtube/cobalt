// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/registration.h"

#include "android_webview/browser/component_updater/loader_policies/aw_apps_package_names_allowlist_component_loader_policy.h"
#include "android_webview/browser/component_updater/loader_policies/empty_component_loader_policy.h"
#include "android_webview/browser/component_updater/origin_trials_component_loader.h"
#include "android_webview/browser/component_updater/trust_token_key_commitments_component_loader.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"

namespace android_webview {

component_updater::ComponentLoaderPolicyVector GetComponentLoaderPolicies() {
  component_updater::ComponentLoaderPolicyVector policies;
  LoadTrustTokenKeyCommitmentsComponent(policies);
  LoadOriginTrialsComponent(policies);
  LoadPackageNamesAllowlistComponent(policies,
                                     AwMetricsServiceClient::GetInstance());
  LoadEmptyComponent(policies);
  return policies;
}

}  // namespace android_webview
