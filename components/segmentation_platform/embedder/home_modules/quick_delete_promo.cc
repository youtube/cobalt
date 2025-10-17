// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/quick_delete_promo.h"

#include "base/metrics/field_trial_params.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_module_utils.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace {

const char kEducationalTipModuleHistogramName[] =
    "MagicStack.Clank.NewTabPage.Module.TopImpressionV2";

// TODO(crbug.com/382803396): The enum id of the quick delete promo card. Could
// be referenced after refactor.
const int kQuickDeletePromoId = 9;

const char kClearBrowsingDataHistogramName[] =
    "Privacy.DeleteBrowsingData.Action";

constexpr std::array<int32_t, 0> kClearBrowsingDataHistogramEnumValues{};
constexpr std::array<int32_t, 0> kEducationalTipModuleHistogramEnumValues{};

const int kClearBrowsingDataHistogramQuickDeleteId = 6;

}  // namespace

namespace segmentation_platform::home_modules {

QuickDeletePromo::QuickDeletePromo(PrefService* profile_prefs)
    : CardSelectionInfo(kQuickDeletePromo), profile_prefs_(profile_prefs) {}

std::map<SignalKey, FeatureQuery> QuickDeletePromo::GetInputs() {
  std::map<SignalKey, FeatureQuery> map = {
      {kIsUserSignedIn,
       FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
           .tensor_length = 1,
           .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
           .name = kIsUserSignedIn})}};

  // Define the number of times user cleared browsing data in the past 30 days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfClearBrowsingData,
                                kClearBrowsingDataHistogramName,
                                kClearBrowsingDataHistogramEnumValues.data(),
                                kClearBrowsingDataHistogramEnumValues.size(),
                                /* days= */ 30);
  map.emplace(kCountOfClearingBrowsingData,
              std::move(countOfClearBrowsingData));

  // Define the number of times user cleared browsing data through quick delete
  // in the past 30 days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfClearBrowsingDataThroughQuickDelete,
                                kClearBrowsingDataHistogramName,
                                &kClearBrowsingDataHistogramQuickDeleteId,
                                /* enum_size= */ 1,
                                /* days= */ 30);
  map.emplace(kCountOfClearingBrowsingDataThroughQuickDelete,
              std::move(countOfClearBrowsingDataThroughQuickDelete));

  int days_to_show_ephemeral_card_once =
      features::KDaysToShowEphemeralCardOnce.Get();
  // Define signal for number of times all educational tip card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(countOfEducationalTipCardShownTimes,
                                kEducationalTipModuleHistogramName,
                                kEducationalTipModuleHistogramEnumValues.data(),
                                kEducationalTipModuleHistogramEnumValues.size(),
                                /* days= */ days_to_show_ephemeral_card_once);
  map.emplace(kEducationalTipShownCount,
              std::move(countOfEducationalTipCardShownTimes));

  int days_to_show_quick_delete_card_once =
      features::KDaysToShowEachEphemeralCardOnce.Get();
  // Define signal for number of times quick delete promo card has shown to the
  // user in limited days.
  DEFINE_UMA_FEATURE_ENUM_COUNT(
      countOfQuickDeletePromoShownTimes, kEducationalTipModuleHistogramName,
      &kQuickDeletePromoId, /* enum_size= */ 1,
      /* days= */ days_to_show_quick_delete_card_once);
  map.emplace(kQuickDeletePromoShownCount,
              std::move(countOfQuickDeletePromoShownTimes));

  return map;
}

CardSelectionInfo::ShowResult QuickDeletePromo::ComputeCardResult(
    const CardSelectionSignals& signals) const {
  // Check for a forced `ShowResult`.
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kQuickDeletePromo == forced_result.value().result_label.value()) {
    return forced_result.value();
  }

  CardSelectionInfo::ShowResult result;
  result.result_label = kQuickDeletePromo;

  bool has_been_interacted_with =
      profile_prefs_->GetBoolean(kQuickDeletePromoInteractedPref);
  if (has_been_interacted_with) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  std::optional<float> result_for_is_user_signed_in =
      signals.GetSignal(kIsUserSignedIn);
  std::optional<float> result_for_count_of_clearing_browsing_data =
      signals.GetSignal(kCountOfClearingBrowsingData);
  std::optional<float>
      result_for_count_of_clearing_browsing_data_through_quick_delete =
          signals.GetSignal(kCountOfClearingBrowsingDataThroughQuickDelete);
  std::optional<float> result_for_quick_delete_promo_shown_count =
      signals.GetSignal(kQuickDeletePromoShownCount);
  std::optional<float>
      result_for_educational_tip_shown_count_for_quick_delete_signal =
          signals.GetSignal(kEducationalTipShownCount);

  if (!result_for_is_user_signed_in.has_value() ||
      !result_for_count_of_clearing_browsing_data.has_value() ||
      !result_for_count_of_clearing_browsing_data_through_quick_delete
           .has_value() ||
      !result_for_quick_delete_promo_shown_count.has_value() ||
      !result_for_educational_tip_shown_count_for_quick_delete_signal
           .has_value()) {
    result.position = EphemeralHomeModuleRank::kNotShown;
    return result;
  }

  // Show the promo card if the user has never cleared browsing data or has
  // cleared browsing data but never through quick delete in the past 30 days
  // and the promo card has not been shown more than 3 times in 24 hours.
  if (*result_for_is_user_signed_in &&
      (result_for_count_of_clearing_browsing_data.value() == 0 ||
       (result_for_count_of_clearing_browsing_data.value() > 0 &&
        result_for_count_of_clearing_browsing_data_through_quick_delete
                .value() == 0)) &&
      result_for_quick_delete_promo_shown_count.value() < 1 &&
      result_for_educational_tip_shown_count_for_quick_delete_signal.value() <
          1) {
    result.position = EphemeralHomeModuleRank::kLast;
    return result;
  }

  result.position = EphemeralHomeModuleRank::kNotShown;
  return result;
}

bool QuickDeletePromo::IsEnabled(bool is_in_enabled_cards_set,
                                 int impression_count) {
  std::optional<CardSelectionInfo::ShowResult> forced_result =
      GetForcedEphemeralModuleShowResult();

  if (forced_result.has_value() &&
      forced_result.value().result_label.has_value() &&
      kQuickDeletePromo == forced_result.value().result_label.value()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(features::kEducationalTipModule) ||
      !is_in_enabled_cards_set) {
    return false;
  }

  if (impression_count >= features::kMaxQuickDeleteCardImpressions.Get()) {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::home_modules
