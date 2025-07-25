// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_utils.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace commerce {
bool UrlContainsDiscountUtmTag(const GURL& url) {
  std::string utm_source;
  std::string utm_medium;
  std::string utm_campaign;
  if (!net::GetValueForKeyInQuery(url, commerce::kUTMSourceLabel,
                                  &utm_source)) {
    return false;
  }
  if (!net::GetValueForKeyInQuery(url, commerce::kUTMMediumLabel,
                                  &utm_medium)) {
    return false;
  }
  if (!net::GetValueForKeyInQuery(url, commerce::kUTMCampaignLabel,
                                  &utm_campaign)) {
    return false;
  }
  return utm_source == commerce::kUTMSourceValue &&
         utm_medium == commerce::kUTMMediumValue &&
         utm_campaign == commerce::kUTMCampaignValueForDiscounts;
}

ParcelTrackingStatus GetParcelTrackingStatusTestData() {
  if (!base::FeatureList::IsEnabled(kParcelTrackingTestData)) {
    NOTREACHED_NORETURN();
  }

  const std::string param = base::GetFieldTrialParamValueByFeature(
      kParcelTrackingTestData, kParcelTrackingTestDataParam);

  ParcelTrackingStatus status;
  status.carrier = commerce::ParcelIdentifier::USPS;
  status.tracking_url = GURL("http://example.com");

  if (param == kParcelTrackingTestDataParamDelivered) {
    status.state = commerce::ParcelStatus::FINISHED;
    status.estimated_delivery_time = base::Time::Now() - base::Hours(1);
  } else if (param == kParcelTrackingTestDataParamInProgress) {
    status.state = commerce::ParcelStatus::WITH_CARRIER;
    status.estimated_delivery_time = base::Time::Now() + base::Hours(48);
  } else if (param == kParcelTrackingTestDataParamOutForDelivery) {
    status.state = commerce::ParcelStatus::OUT_FOR_DELIVERY;
    status.estimated_delivery_time = base::Time::Now() + base::Hours(1);
  }

  return status;
}

}  // namespace commerce
