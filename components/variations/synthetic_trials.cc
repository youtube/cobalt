// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trials.h"
#include "components/variations/hashing.h"

namespace variations {

SyntheticTrialGroup::SyntheticTrialGroup(
    base::StringPiece trial_name,
    base::StringPiece group_name,
    SyntheticTrialAnnotationMode annotation_mode)
    : annotation_mode_(annotation_mode) {
  SetTrialName(trial_name);
  SetGroupName(group_name);
}

SyntheticTrialGroup::SyntheticTrialGroup(const SyntheticTrialGroup&) = default;

void SyntheticTrialGroup::SetTrialName(base::StringPiece trial_name) {
  active_group_.trial_name = std::string(trial_name);
  id_.name = variations::HashName(trial_name);
}

void SyntheticTrialGroup::SetGroupName(base::StringPiece group_name) {
  active_group_.group_name = std::string(group_name);
  id_.group = variations::HashName(group_name);
}

}  // namespace variations
