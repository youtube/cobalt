// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace autofill {
namespace features {

// All features in alphabetical order.
BASE_DECLARE_FEATURE(kAutofillEnableAndroidNKeyForFidoAuthentication);
BASE_DECLARE_FEATURE(kAutofillEnableCardArtImage);
BASE_DECLARE_FEATURE(kAutofillEnableCardArtServerSideStretching);
BASE_DECLARE_FEATURE(kAutofillEnableCardProductName);
BASE_DECLARE_FEATURE(kAutofillEnableCvcStorageAndFilling);
BASE_DECLARE_FEATURE(kAutofillEnableEmailOtpForVcnYellowPath);
BASE_DECLARE_FEATURE(kAutofillEnableFIDOProgressDialog);
BASE_DECLARE_FEATURE(kAutofillEnableFpanRiskBasedAuthentication);
BASE_DECLARE_FEATURE(kAutofillEnableManualFallbackForVirtualCards);
BASE_DECLARE_FEATURE(kAutofillEnableMerchantDomainInUnmaskCardRequest);
BASE_DECLARE_FEATURE(kAutofillEnableMerchantOptOutClientSideUrlFiltering);
BASE_DECLARE_FEATURE(kAutofillEnableMovingGPayLogoToTheRightOnDesktop);
BASE_DECLARE_FEATURE(kAutofillEnableMovingGPayLogoToTheRightOnClank);
BASE_DECLARE_FEATURE(kAutofillEnableNewCardArtAndNetworkImages);
BASE_DECLARE_FEATURE(kAutofillEnableNewSaveCardBubbleUi);
BASE_DECLARE_FEATURE(kAutofillEnableOffersInClankKeyboardAccessory);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAutofillEnablePaymentsAndroidBottomSheet);
#endif

BASE_DECLARE_FEATURE(kAutofillEnablePaymentsMandatoryReauth);
BASE_DECLARE_FEATURE(kAutofillEnableRemadeDownstreamMetrics);
BASE_DECLARE_FEATURE(kAutofillEnableServerIban);
BASE_DECLARE_FEATURE(kAutofillEnableStickyManualFallbackForCards);
BASE_DECLARE_FEATURE(kAutofillEnableUpdateVirtualCardEnrollment);
BASE_DECLARE_FEATURE(kAutofillEnableVirtualCardFidoEnrollment);
BASE_DECLARE_FEATURE(kAutofillEnableVirtualCardMetadata);
BASE_DECLARE_FEATURE(kAutofillMoveLegalTermsAndIconForNewCardEnrollment);
BASE_DECLARE_FEATURE(kAutofillParseVcnCardOnFileStandaloneCvcFields);
BASE_DECLARE_FEATURE(kAutofillSuggestServerCardInsteadOfLocalCard);
BASE_DECLARE_FEATURE(kAutofillUpdateChromeSettingsLinkToGPayWeb);
BASE_DECLARE_FEATURE(kAutofillUpstream);
BASE_DECLARE_FEATURE(kAutofillUpstreamAllowAdditionalEmailDomains);
BASE_DECLARE_FEATURE(kAutofillUpstreamAllowAllEmailDomains);

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kAutofillUseTwoDotsForLastFourDigits);
BASE_DECLARE_FEATURE(kAutofillEnablePaymentsMandatoryReauthOnBling);
BASE_DECLARE_FEATURE(kAutofillEnableVirtualCards);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kEnablePixPayments);
#endif

// Return whether a [No thanks] button and new messaging is shown in the save
// card bubbles. This will be called only on desktop platforms.
bool ShouldShowImprovedUserConsentForCreditCardSave();

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
