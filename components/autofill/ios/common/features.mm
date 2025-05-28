// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

// Features that are exlusive to iOS go here in alphabetical order.

BASE_FEATURE(kAddAddressManually,
             "AddAdressManually",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAddAddressManuallyEnabled() {
  return base::FeatureList::IsEnabled(kAddAddressManually) &&
         base::FeatureList::IsEnabled(
             kAutofillDynamicallyLoadsFieldsForAddressInput);
}

BASE_FEATURE(kAutofillDynamicallyLoadsFieldsForAddressInput,
             "AutofillDynamicallyLoadsFieldsForAddressInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_fix_post_filling_payment_sheet)
BASE_FEATURE(kAutofillFixPaymentSheetSpam,
             "AutofillFixPostFillingPaymentSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_fix_post_filling_payment_sheet)

// LINT.IfChange(autofill_isolated_content_world)
BASE_FEATURE(kAutofillIsolatedWorldForJavascriptIos,
             "AutofillIsolatedWorldForJavascriptIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_isolated_content_world)

BASE_FEATURE(kAutofillPaymentsSheetV2Ios,
             "AutofillPaymentsSheetV2Ios",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillPaymentsSheetV3Ios,
             "AutofillPaymentsSheetV3Ios",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillStickyInfobarIos,
             "AutofillStickyInfobarIos",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillThrottleDocumentFormScanIos,
             "AutofillThrottleDocumentFormScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the document form scanning batches.
extern const base::FeatureParam<int> kAutofillDocumentFormScanPeriodMs = {
    &kAutofillThrottleDocumentFormScanIos,
    /*name=*/"period-ms", /*default_value=*/250};

BASE_FEATURE(kAutofillThrottleDocumentFormScanForceFirstScanIos,
             "AutofillThrottleDocumentFormScanForceFirstScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillThrottleFilteredDocumentFormScanIos,
             "AutofillThrottleFilteredDocumentFormScanIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Minimal period of time between the filtered document form scanning batches.
extern const base::FeatureParam<int> kAutofillFilteredDocumentFormScanPeriodMs =
    {&kAutofillThrottleFilteredDocumentFormScanIos,
     /*name=*/"period-ms", /*default_value=*/250};
