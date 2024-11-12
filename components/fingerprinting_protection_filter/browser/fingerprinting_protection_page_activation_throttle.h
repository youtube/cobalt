// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PAGE_ACTIVATION_THROTTLE_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PAGE_ACTIVATION_THROTTLE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "content/public/browser/navigation_throttle.h"

class PrefService;

namespace privacy_sandbox {
class TrackingProtectionSettings;
}

namespace subresource_filter {
enum class ActivationDecision;
namespace mojom {
enum class ActivationLevel;
}  // namespace mojom
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

struct GetActivationResult {
  subresource_filter::mojom::ActivationLevel level;
  subresource_filter::ActivationDecision decision;
};

// Navigation throttle responsible for activating subresource filtering on page
// loads that match the Fingerprinting Protection Filtering criteria.
class FingerprintingProtectionPageActivationThrottle
    : public content::NavigationThrottle {
 public:
  FingerprintingProtectionPageActivationThrottle(
      content::NavigationHandle* handle,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      PrefService* prefs,
      bool is_incognito = false);

  FingerprintingProtectionPageActivationThrottle(
      const FingerprintingProtectionPageActivationThrottle&) = delete;
  FingerprintingProtectionPageActivationThrottle& operator=(
      const FingerprintingProtectionPageActivationThrottle&) = delete;

  ~FingerprintingProtectionPageActivationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  bool GetEnablePerformanceMeasurements(bool is_incognito) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(FPFPageActivationThrottleTestGetActivationTest,
                           GetActivationComputesLevelAndDecision);

  // Computes the ActivationLevel and ActivationDecision for the current URL
  // based on feature flags/params and prefs. This function is necessary because
  // there is some interaction between flags/params and prefs.
  GetActivationResult GetActivation() const;

  void CheckCurrentUrl();

  virtual void NotifyResult(GetActivationResult activation_result);

  // Helper function to abstract getting the WebContentsHelper dependency.
  // This structure is useful for testing.
  virtual void NotifyPageActivationComputed(
      subresource_filter::mojom::ActivationState activation_state,
      subresource_filter::ActivationDecision activation_decision);

  void LogMetricsOnChecksComplete(
      subresource_filter::ActivationDecision decision,
      subresource_filter::mojom::ActivationLevel level) const;

  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  raw_ptr<PrefService> prefs_;

  // Set to TimeTicks::Now() when the navigation is deferred in
  // WillProcessResponse. If deferral was not necessary, will remain null.
  base::TimeTicks defer_time_;

  // Whether this throttle is deferring the navigation. Only set to true in
  // WillProcessResponse if there are ongoing fingerprinting blocking checks.
  bool deferring_ = false;

  // Whether the profile is in Incognito mode.
  bool is_incognito_;

  base::WeakPtrFactory<FingerprintingProtectionPageActivationThrottle>
      weak_ptr_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_PAGE_ACTIVATION_THROTTLE_H_
