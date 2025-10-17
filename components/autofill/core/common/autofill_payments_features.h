// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace autofill::features {

// All features in alphabetical order.
#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableDefaultSaveCardFixFlowDetection);
#endif
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableAllowlistForBmoCardCategoryBenefits);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableAmountExtractionAllowlistDesktop);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableAmountExtractionDesktop);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableAmountExtractionDesktopLogging);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableBuyNowPayLater);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableBuyNowPayLaterSyncing);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCardBenefitsForAmericanExpress);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCardBenefitsForBmo);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCardBenefitsIph);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCardBenefitsSourceSync);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCardBenefitsSync);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCardInfoRuntimeRetrieval);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCvcStorageAndFilling);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCvcStorageAndFillingEnhancement);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(
    kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableFlatRateCardBenefitsBlocklist);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableFlatRateCardBenefitsFromCurinos);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableFpanRiskBasedAuthentication);

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableLogFormEventsToAllParsedFormTypes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(
    kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableNewCardBenefitsToggleText);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableNewFopDisplayDesktop);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableOffersInClankKeyboardAccessory);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnablePaymentSettingsCardPromoAndScanCard);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnablePaymentSettingsServerCardSave);
#endif

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnablePrefetchingRiskDataForRetrieval);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSaveAndFill);
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableShowSaveCardSecurelyMessage);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSyncingOfPixBankAccounts);
#endif

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableVcn3dsAuthentication);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableVirtualCardJavaPaymentsDataManager);
#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillLocalSaveCardBottomSheet);
#endif
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillParseVcnCardOnFileStandaloneCvcFields);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillRetryImageFetchOnFailure);
#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSaveCardBottomSheet);
#endif
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSharedStorageServerCardData);
#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillShowManualFillForVirtualCards);
#endif
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSkipSaveCardForTabModalPopup);
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillSyncEwalletAccounts);
#endif

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUnmaskCardRequestTimeout);

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUploadCardRequestTimeout);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillUploadCardRequestTimeoutMilliseconds;

COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillUpstream);

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillVcnEnrollRequestTimeout);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillVcnEnrollRequestTimeoutMilliseconds;

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillVcnEnrollStrikeExpiryTime);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillVcnEnrollStrikeExpiryTimeDays;

// Return whether a [No thanks] button and new messaging is shown in the save
// card bubbles. This will be called only on desktop platforms.
COMPONENT_EXPORT(AUTOFILL)
bool ShouldShowImprovedUserConsentForCreditCardSave();

}  // namespace autofill::features

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
