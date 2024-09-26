// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace autofill::features {

// All features in alphabetical order.
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillAcrossIframes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(
    kAutofillAddressProfileSavePromptAddressVerificationSupport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillGivePrecedenceToNumericQuantities);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAccountProfilesUnionView);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillEnableSilentUpdatesForAccountProfiles;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAccountProfileStorage);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAccountProfileStorageFromUnsupportedIPs;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAddressProfileSavePromptNicknameSupport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAllowDuplicateFormSubmissions);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillAssociateForms);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<base::TimeDelta> kAutofillAssociateFormsTTL;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillInferCountryCallingCode);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillConsiderPhoneNumberSeparatorsValidLabels);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDeferSubmissionClassificationAfterAjax);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDontPreserveAutofillState);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDelayBlurVotes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSelectMenu);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableWithinFencedFrame);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillFillAndImportFromMoreFields);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillFillAutocompleteUnrecognized;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillImportFromAutocompleteUnrecognized;
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableFilling);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableAddressImport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableShadowHeuristics);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableBirthdateParsing);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableDependentLocalityParsing);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableDevtoolsIssues);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableParsingEmptyPhoneNumberLabels);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableRankingFormulaAddressProfiles);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaAddressProfilesUsageHalfLife;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableRankingFormulaCreditCards);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaCreditCardsUsageHalfLife;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoost;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaVirtualCardBoostHalfLife;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForApartmentNumbers);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableProfileDeduplication);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForMergingSubsetNames);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForHonorificPrefixes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillExtractAllDatalists);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForPhoneNumberTrunkTypes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillFeedback);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillOnlyCacheIsAutofilledOnFill);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillConvergeToExtremeLengthStreetAddress);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillConvergeToLonger;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(
    kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete);
// Records the values of HtmlFieldType that the server or heuristic predictions
// will be able to override when the feature
// `kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete` is enabled.
enum class PrecedenceOverAutocompleteScope {
  kNone = 0,
  kAddressLine1And2 = 1,
  kRecognized = 2,
  kSpecified = 3,
  kMaxValue = kSpecified
};
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<PrecedenceOverAutocompleteScope>
    kAutofillHeuristicPrecedenceScopeOverAutocomplete;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<PrecedenceOverAutocompleteScope>
    kAutofillServerPrecedenceScopeOverAutocomplete;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillIgnoreUnmappableAutocompleteValues);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillHighlightOnlyChangedValuesInPreviewMode);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillLabelAffixRemoval);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillPageLanguageDetection);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillParseAsync);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillParseNameAsAutocompleteType);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillParsingPatternProvider);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillParsingPatternActiveSource;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAlwaysParsePlaceholders);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPopupUseThresholdForKeyboardAndMobileAccept);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPreventOverridingPrefilledValues);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillProbableFormSubmissionInBrowser);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillRationalizeStreetAddressAndHouseNumber);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillRemoveInaccessibleProfileValuesOnStartup);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillRequireNameForProfileImport);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillSaveAndFillVPA);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillServerBehaviors);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillServerBehaviorsParam;
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillSharedAutofill);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillSharedAutofillRelaxedParam;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillShowAutocompleteDeleteButton);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillShowManualFallbackInContextMenu);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSkipComparingInferredLabels);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSplitCreditCardNumbersCautiously);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillTokenPrefixMatching);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseAlternativeStateNameMap);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseImprovedLabelDisambiguation);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseNewSectioningMethod);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseParameterizedSectioning);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillSectioningModeIgnoreAutocomplete;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillSectioningModeCreateGaps;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillSectioningModeExpand;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillSectioningModeExpandOverUnfocusableFields;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillRefillByFormRendererId);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillEnableAblationStudy);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForAddressesParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForPaymentsParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleParam;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillVoteForSelectOptionValues);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillMoreProminentPopupMaxOffsetToCenterParam;
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillMoreProminentPopup);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillLogUKMEventsWithSampleRate);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseUpdatedRequiredFieldsForAddressImport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillVirtualCardsOnTouchToFillAndroid);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillKeyboardAccessory);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillManualFallbackAndroid);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillTouchToFillForCreditCardsAndroid);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseMobileLabelDisambiguation);
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterName[];
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterShowOne[];
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterShowAll[];
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_APPLE)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
COMPONENT_EXPORT(AUTOFILL)
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
bool IsAutofillManualFallbackEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

// The features in this namespace contains are not meant to be rolled out. They
// are are only intended for manual testing purposes.
namespace test {

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAllowNonHttpActivation);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCapturedSiteTestsMetricsScraper);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillCapturedSiteTestsMetricsScraperOutputDir;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillCapturedSiteTestsMetricsScraperHistogramRegex;
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableProfileUpdates);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableSilentProfileUpdates);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCreateAccountProfilesFromSettings);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillLogToTerminal);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillServerCommunication);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillShowTypePredictions);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillUploadThrottling);

}  // namespace test

}  // namespace autofill::features

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
