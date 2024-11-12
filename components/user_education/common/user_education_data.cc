// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/user_education_data.h"

namespace user_education {

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoClosedReason close_reason) {
  switch (close_reason) {
    case FeaturePromoClosedReason::kDismiss:
      oss << "kDismiss";
      break;
    case FeaturePromoClosedReason::kSnooze:
      oss << "kSnooze";
      break;
    case FeaturePromoClosedReason::kAction:
      oss << "kAction";
      break;
    case FeaturePromoClosedReason::kCancel:
      oss << "kCancel";
      break;
    case FeaturePromoClosedReason::kTimeout:
      oss << "kTimeout";
      break;
    case FeaturePromoClosedReason::kAbortPromo:
      oss << "kAbortPromo";
      break;
    case FeaturePromoClosedReason::kFeatureEngaged:
      oss << "kFeatureEngaged";
      break;
    case FeaturePromoClosedReason::kOverrideForUIRegionConflict:
      oss << "kOverrideForUIRegionConflict";
      break;
    case FeaturePromoClosedReason::kOverrideForDemo:
      oss << "kOverrideForDemo";
      break;
    case FeaturePromoClosedReason::kOverrideForTesting:
      oss << "kOverrideForTesting";
      break;
    case FeaturePromoClosedReason::kOverrideForPrecedence:
      oss << "kOverrideForPrecedence";
      break;
    case FeaturePromoClosedReason::kAbortedByFeature:
      oss << "kAbortedByFeature";
      break;
    case FeaturePromoClosedReason::kAbortedByAnchorHidden:
      oss << "kAbortedByAnchorHidden";
      break;
    case FeaturePromoClosedReason::kAbortedByBubbleDestroyed:
      oss << "kAbortedByBubbleDestroyed";
      break;
  }
  return oss;
}

FeaturePromoData::FeaturePromoData() = default;
FeaturePromoData::FeaturePromoData(const FeaturePromoData&) = default;
FeaturePromoData::FeaturePromoData(FeaturePromoData&&) noexcept = default;
FeaturePromoData& FeaturePromoData::operator=(const FeaturePromoData&) =
    default;
FeaturePromoData& FeaturePromoData::operator=(FeaturePromoData&&) noexcept =
    default;
FeaturePromoData::~FeaturePromoData() = default;

UserEducationSessionData::UserEducationSessionData() = default;
UserEducationSessionData::UserEducationSessionData(
    const UserEducationSessionData&) = default;
UserEducationSessionData::UserEducationSessionData(
    UserEducationSessionData&&) noexcept = default;
UserEducationSessionData& UserEducationSessionData::operator=(
    const UserEducationSessionData&) = default;
UserEducationSessionData& UserEducationSessionData::operator=(
    UserEducationSessionData&&) noexcept = default;
UserEducationSessionData::~UserEducationSessionData() = default;

FeaturePromoPolicyData::FeaturePromoPolicyData() = default;
FeaturePromoPolicyData::FeaturePromoPolicyData(const FeaturePromoPolicyData&) =
    default;
FeaturePromoPolicyData::FeaturePromoPolicyData(
    FeaturePromoPolicyData&&) noexcept = default;
FeaturePromoPolicyData& FeaturePromoPolicyData::operator=(
    const FeaturePromoPolicyData&) = default;
FeaturePromoPolicyData& FeaturePromoPolicyData::operator=(
    FeaturePromoPolicyData&&) noexcept = default;
FeaturePromoPolicyData::~FeaturePromoPolicyData() = default;

ProductMessagingData::ProductMessagingData() = default;
ProductMessagingData::ProductMessagingData(const ProductMessagingData&) =
    default;
ProductMessagingData::ProductMessagingData(ProductMessagingData&&) noexcept =
    default;
ProductMessagingData& ProductMessagingData::operator=(
    const ProductMessagingData&) = default;
ProductMessagingData& ProductMessagingData::operator=(
    ProductMessagingData&&) noexcept = default;
ProductMessagingData::~ProductMessagingData() = default;

}  // namespace user_education
