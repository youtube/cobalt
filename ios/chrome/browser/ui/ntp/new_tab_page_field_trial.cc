// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/strcat.h"
#import "components/ntp_tiles/features.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ui/first_run/ios_first_run_field_trials.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial_constants.h"
#import "ios/chrome/common/channel_info.h"

namespace {

// The placeholder trial version that is stored for a client who has not been
// enrolled in the experiment.
const int kPlaceholderTrialVersion = -1;

// Store local state preference with whether the client has participated in
// kNewTabPageFieldTrial experiment or not.
const char kTrialPrefName[] = "new_tab_page.trial_version";

// The current trial version of
// kNewTabPageFieldTrial; should be updated when
// the experiment is modified.
const int kCurrentTrialVersion = 1;

// Returns a map of the group weights for each arm.
std::map<variations::VariationID, int> GetGroupWeights() {
  std::map<variations::VariationID, int> weight_by_id = {
      // Tile ablation groups
      {field_trial_constants::kTileAblationHideAllID, 0},
      {field_trial_constants::kTileAblationHideOnlyMVTID, 0},
      {field_trial_constants::kTileAblationControlID, 0}};

  switch (GetChannel()) {
    case version_info::Channel::UNKNOWN:
      [[fallthrough]];
    case version_info::Channel::CANARY:
      [[fallthrough]];
    case version_info::Channel::DEV:
      [[fallthrough]];
    case version_info::Channel::BETA:
      // Tile ablation groups.
      weight_by_id[field_trial_constants::kTileAblationHideAllID] =
          field_trial_constants::kTileAblationPrestableWeight;
      weight_by_id[field_trial_constants::kTileAblationHideOnlyMVTID] =
          field_trial_constants::kTileAblationPrestableWeight;
      weight_by_id[field_trial_constants::kTileAblationControlID] =
          field_trial_constants::kTileAblationPrestableWeight;
      break;
    case version_info::Channel::STABLE:
      // Tile ablation groups.
      weight_by_id[field_trial_constants::kTileAblationHideAllID] =
          field_trial_constants::kTileAblationStableWeight;
      weight_by_id[field_trial_constants::kTileAblationHideOnlyMVTID] =
          field_trial_constants::kTileAblationStableWeight;
      weight_by_id[field_trial_constants::kTileAblationControlID] =
          field_trial_constants::kTileAblationStableWeight;
      break;
  }

  return weight_by_id;
}

}  // namespace

namespace new_tab_page_field_trial {

// Creates the trial config, initializes the trial that puts clients into
// different groups, and returns the version number of the current trial. There
// are 3 groups other than the default group:
// - Tile ablation, control group.
// - Tile ablation, hide all tiles group.
// - Tile ablation, hide only MVT group.
void CreateNewTabPageFieldTrial(
    std::map<variations::VariationID, int> weight_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  FirstRunFieldTrialConfig config(ntp_tiles::kNewTabPageFieldTrial.name);

  // Tile ablation group that hides all tiles.
  config.AddGroup(
      base::StrCat({field_trial_constants::kTileAblationHideAllGroup,
                    field_trial_constants::
                        kNewTabPageFieldTrialVersionSuffixTileAblation}),
      field_trial_constants::kTileAblationHideAllID,
      weight_by_id[field_trial_constants::kTileAblationHideAllID]);
  base::FieldTrialParams tile_ablation_hide_all_params;
  tile_ablation_hide_all_params[ntp_tiles::kNewTabPageFieldTrialParam] = "1";
  base::AssociateFieldTrialParams(
      ntp_tiles::kNewTabPageFieldTrial.name,
      base::StrCat({field_trial_constants::kTileAblationHideAllGroup,
                    field_trial_constants::
                        kNewTabPageFieldTrialVersionSuffixTileAblation}),
      tile_ablation_hide_all_params);

  // Tile ablation group that hides only MVT tiles.
  config.AddGroup(
      base::StrCat({field_trial_constants::kTileAblationHideOnlyMVTGroup,
                    field_trial_constants::
                        kNewTabPageFieldTrialVersionSuffixTileAblation}),
      field_trial_constants::kTileAblationHideOnlyMVTID,
      weight_by_id[field_trial_constants::kTileAblationHideOnlyMVTID]);
  base::FieldTrialParams tile_ablation_hide_mvt_params;
  tile_ablation_hide_mvt_params[ntp_tiles::kNewTabPageFieldTrialParam] = "2";
  base::AssociateFieldTrialParams(
      ntp_tiles::kNewTabPageFieldTrial.name,
      base::StrCat({field_trial_constants::kTileAblationHideOnlyMVTGroup,
                    field_trial_constants::
                        kNewTabPageFieldTrialVersionSuffixTileAblation}),
      tile_ablation_hide_mvt_params);

  // Tile ablation control group.
  config.AddGroup(
      base::StrCat({field_trial_constants::kTileAblationControlGroup,
                    field_trial_constants::
                        kNewTabPageFieldTrialVersionSuffixTileAblation}),
      field_trial_constants::kTileAblationControlID,
      weight_by_id[field_trial_constants::kTileAblationControlID]);
  base::FieldTrialParams tile_ablation_control_params;
  tile_ablation_control_params[ntp_tiles::kNewTabPageFieldTrialParam] = "3";
  base::AssociateFieldTrialParams(
      ntp_tiles::kNewTabPageFieldTrial.name,
      base::StrCat({field_trial_constants::kTileAblationControlGroup,
                    field_trial_constants::
                        kNewTabPageFieldTrialVersionSuffixTileAblation}),
      tile_ablation_control_params);

  // Default group.
  scoped_refptr<base::FieldTrial> trial = config.CreateOneTimeRandomizedTrial(
      field_trial_constants::kNewTabPageFieldTrialDefaultGroup,
      low_entropy_provider);

  //   Finalize the group choice and activates the trial - similar to a
  //   variation config that's marked with `starts_active` true. This is
  //   required for studies that register variation ids, so they don't reveal
  //   extra information beyond the low-entropy source.
  const std::string& group_name = trial->group_name();

  if (group_name ==
          base::StrCat({field_trial_constants::kTileAblationControlGroup,
                        field_trial_constants::
                            kNewTabPageFieldTrialVersionSuffixTileAblation}) ||
      group_name == field_trial_constants::kNewTabPageFieldTrialDefaultGroup) {
    feature_list->RegisterFieldTrialOverride(
        ntp_tiles::kNewTabPageFieldTrial.name,
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial.get());
  } else {
    feature_list->RegisterFieldTrialOverride(
        ntp_tiles::kNewTabPageFieldTrial.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
  }
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTrialPrefName, kPlaceholderTrialVersion);
}

void Create(const base::FieldTrial::EntropyProvider& low_entropy_provider,
            base::FeatureList* feature_list,
            PrefService* local_state) {
  // Don't create the trial if the feature is overridden to avoid having
  // multiple registered trials for the same feature.
  if (feature_list->IsFeatureOverridden(
          ntp_tiles::kNewTabPageFieldTrial.name)) {
    return;
  }

  // If the client is already an existing client by the time this experiment
  // began running, don't register (e.g. the client is not in a First Run
  // experience and was never grouped client-side into this study when it went
  // through First Run).

  // If the user is enrolled in a previous version of the same experiment,
  // exclude them out of the current version.
  if (!FirstRun::IsChromeFirstRun() &&
      local_state->GetInteger(kTrialPrefName) != kCurrentTrialVersion) {
    return;
  }

  // Enroll first run clients in the experiment.
  // If the client is enrolled in the current version of the experiment,
  // register the trial to keep them in the experiment; they will be placed
  // in the same group because `low_entropy_provider` is persisted across
  // launches.
  CreateNewTabPageFieldTrial(GetGroupWeights(), low_entropy_provider,
                             feature_list);

  local_state->SetInteger(kTrialPrefName, kCurrentTrialVersion);
}

void CreateNewTabPageFieldTrialForTesting(
    std::map<variations::VariationID, int> weights_by_id,
    const base::FieldTrial::EntropyProvider& low_entropy_provider,
    base::FeatureList* feature_list) {
  CreateNewTabPageFieldTrial(weights_by_id, low_entropy_provider, feature_list);
}

}  // namespace new_tab_page_field_trial
