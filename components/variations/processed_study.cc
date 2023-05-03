// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include <set>
#include <string>

#include "base/version.h"
#include "components/variations/proto/study.pb.h"

namespace variations {

namespace {

// Validates the sanity of |study| and computes the total probability and
// whether all assignments are to a single group.
bool ValidateStudyAndComputeTotalProbability(
    const Study& study,
    base::FieldTrial::Probability* total_probability,
    bool* all_assignments_to_one_group,
    std::vector<std::string>* associated_features) {
  if (study.filter().has_min_version() &&
      !base::Version::IsValidWildcardString(study.filter().min_version())) {
    DVLOG(1) << study.name() << " has invalid min version: "
             << study.filter().min_version();
    return false;
  }
  if (study.filter().has_max_version() &&
      !base::Version::IsValidWildcardString(study.filter().max_version())) {
    DVLOG(1) << study.name() << " has invalid max version: "
             << study.filter().max_version();
    return false;
  }

  const std::string& default_group_name = study.default_experiment_name();
  base::FieldTrial::Probability divisor = 0;

  bool multiple_assigned_groups = false;
  bool found_default_group = false;

  std::set<std::string> experiment_names;
  std::set<std::string> features_to_associate;

  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.name().empty()) {
      DVLOG(1) << study.name() << " is missing experiment " << i << " name";
      return false;
    }
    if (!experiment_names.insert(experiment.name()).second) {
      DVLOG(1) << study.name() << " has a repeated experiment name "
               << study.experiment(i).name();
      return false;
    }

    // Note: This checks for ACTIVATION_EXPLICIT, since there is no reason to
    // have this association with ACTIVATION_AUTO (where the trial starts
    // active), as well as allowing flexibility to disable this behavior in the
    // future from the server by introducing a new activation type.
    if (study.activation_type() == Study_ActivationType_ACTIVATION_EXPLICIT) {
      const auto& features = experiment.feature_association();
      for (int i = 0; i < features.enable_feature_size(); ++i) {
        features_to_associate.insert(features.enable_feature(i));
      }
      for (int i = 0; i < features.disable_feature_size(); ++i) {
        features_to_associate.insert(features.disable_feature(i));
      }
    }

    if (!experiment.has_forcing_flag() && experiment.probability_weight() > 0) {
      // If |divisor| is not 0, there was at least one prior non-zero group.
      if (divisor != 0)
        multiple_assigned_groups = true;
      divisor += experiment.probability_weight();
    }
    if (study.experiment(i).name() == default_group_name)
      found_default_group = true;
  }

  // Specifying a default experiment is optional, so finding it in the
  // experiment list is only required when it is specified.
  if (!study.default_experiment_name().empty() && !found_default_group) {
    DVLOG(1) << study.name() << " is missing default experiment ("
             << study.default_experiment_name() << ") in its experiment list";
    // The default group was not found in the list of groups. This study is not
    // valid.
    return false;
  }

  // Ensure that groups that don't explicitly enable/disable any features get
  // associated with all features in the study (i.e. so "Default" group gets
  // reported).
  if (!features_to_associate.empty()) {
    associated_features->insert(associated_features->end(),
                                features_to_associate.begin(),
                                features_to_associate.end());
  }

  *total_probability = divisor;
  *all_assignments_to_one_group = !multiple_assigned_groups;
  return true;
}


}  // namespace

// static
const char ProcessedStudy::kGenericDefaultExperimentName[] =
    "VariationsDefaultExperiment";

ProcessedStudy::ProcessedStudy() {}

ProcessedStudy::ProcessedStudy(const ProcessedStudy& other) = default;

ProcessedStudy::~ProcessedStudy() {
}

bool ProcessedStudy::Init(const Study* study, bool is_expired) {
  base::FieldTrial::Probability total_probability = 0;
  bool all_assignments_to_one_group = false;
  std::vector<std::string> associated_features;
  if (!ValidateStudyAndComputeTotalProbability(*study, &total_probability,
                                               &all_assignments_to_one_group,
                                               &associated_features)) {
    return false;
  }

  study_ = study;
  is_expired_ = is_expired;
  total_probability_ = total_probability;
  all_assignments_to_one_group_ = all_assignments_to_one_group;
  associated_features_.swap(associated_features);
  return true;
}

int ProcessedStudy::GetExperimentIndexByName(const std::string& name) const {
  for (int i = 0; i < study_->experiment_size(); ++i) {
    if (study_->experiment(i).name() == name)
      return i;
  }

  return -1;
}

const char* ProcessedStudy::GetDefaultExperimentName() const {
  if (study_->default_experiment_name().empty())
    return kGenericDefaultExperimentName;

  return study_->default_experiment_name().c_str();
}

// static
bool ProcessedStudy::ValidateAndAppendStudy(
    const Study* study,
    bool is_expired,
    std::vector<ProcessedStudy>* processed_studies) {
  ProcessedStudy processed_study;
  if (processed_study.Init(study, is_expired)) {
    processed_studies->push_back(processed_study);
    return true;
  }
  return false;
}

}  // namespace variations
