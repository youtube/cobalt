// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <initializer_list>
#include <map>
#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

// For these tests, we use a small LES range to make the expectations simpler.
const uint32_t kMaxEntropy = 20;
const uint32_t kLayerId = 1;
const uint32_t kLayerSalt = kLayerId;
const char kStudyName[] = "Uniformity";

struct LayerStudySeedOptions {
  bool layer_constrain_study = true;
  bool force_low_entropy_layer = true;
  bool force_low_entropy = false;
  uint32_t salt = kLayerSalt;
  uint32_t slot_multiplier = 1;
};

// Generates a seed for checking uniformity of assignments in layered
// constrained study. This seed contains a 3 arm study active in 9/10 slots.
// |slot_multiplier| increased the number of slots in each range, but should not
// affect randomization.
VariationsSeed LayerStudySeed(LayerStudySeedOptions options) {
  VariationsSeed seed;
  Layer* layer = seed.add_layers();
  layer->set_id(kLayerId);
  layer->set_salt(options.salt);
  layer->set_num_slots(10 * options.slot_multiplier);
  if (options.force_low_entropy_layer)
    layer->set_entropy_mode(Layer::LOW);
  Layer::LayerMember* member = layer->add_members();
  member->set_id(82);
  // Use a 9/10 slots, but with the slots into a 4 and 5 slot chunk that are
  // discontiguous. Neither range alone will allow uniform randomization if
  // 3 does not divide the range of the LES values.
  Layer::LayerMember::SlotRange* slot = member->add_slots();
  slot->set_start(0 * options.slot_multiplier);
  slot->set_end(4 * options.slot_multiplier);
  slot = member->add_slots();
  slot->set_start(6 * options.slot_multiplier);
  slot->set_end(9 * options.slot_multiplier);

  Study* study = seed.add_study();
  study->set_name(kStudyName);
  study->set_consistency(Study_Consistency_PERMANENT);

  if (options.layer_constrain_study) {
    LayerMemberReference* layer_membership = study->mutable_layer();
    layer_membership->set_layer_id(kLayerId);
    layer_membership->set_layer_member_id(82);
  }
  // Use 3 arms, which does not divide
  for (auto* group_name : {"A", "B", "C"}) {
    auto* exp = study->add_experiment();
    exp->set_name(group_name);
    exp->set_probability_weight(1);
  }
  if (options.force_low_entropy) {
    study->mutable_experiment(0)->set_google_web_experiment_id(12345);
  }
  return seed;
}

// When assigned directly from low entropy, the following assignments are used
// for the study.
const std::vector<std::string> kExpectedLowEntropyAssignments = {
    "C", "A", "B", "C", "A", "B", "C", "A", "B", "B",  // 10
    "B", "A", "C", "C", "C", "A", "B", "B", "A", "A",  // 20
};

// LayeredStudySeed should give the following assignment using the test LES.
// All 3 arms get 6/20 values, with 2/20 not in the study.
const std::vector<std::string> kExpectedRemainderEntropyAssignments = {
    "A", "",  "B", "B", "C", "A", "B", "B", "C", "C",  // 10
    "C", "A", "",  "A", "C", "A", "C", "B", "A", "B",  // 20
};

// The expected group assignments for the study based on high entropy.
// This does not take into account any layer exclusions.
// This is only a small sample of the entropy space, so we don't expect
// precised uniformity, just a reasonable mixture.
const std::vector<std::string> kExpectedHighEntropyStudyAssignments = {
    "C", "B", "A", "B", "A", "B", "A", "A", "B", "C",  // 10
    "B", "A", "A", "C", "C", "A", "A", "B", "B", "C",  // 20
    "C", "A", "C", "A", "C", "A", "B", "B", "A", "B",  // 30
    "A", "B", "C", "B", "B", "C", "C", "C", "B", "B",  // 40
    "C", "B", "B", "C", "C", "B", "A", "B", "C", "C",  // 50
    "C", "B", "C", "C", "B", "B", "C", "B", "A", "A",  // 60
    "B", "C", "A", "C", "A", "B", "B", "C", "B", "A",  // 70
    "B", "B", "A", "B", "A", "C", "B", "A", "B", "B",  // 80
    "B", "A", "B", "C", "C", "B", "A", "A", "C", "A",  // 90
    "A", "A", "C", "B", "B", "C", "B", "C", "A", "C",  // 100
};

// Process the seed and return which group the user is assigned for Uniformity.
std::string GetUniformityAssignment(const VariationsSeed& seed,
                                    const EntropyProviders& entropy_providers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Init();
  base::FeatureList feature_list;
  auto client_state = CreateDummyClientFilterableState();
  // This should mimic the call through SetUpFieldTrials from
  // android_webview/browser/aw_feature_list_creator.cc
  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_state, base::BindRepeating(NoopUIStringOverrideCallback),
      entropy_providers, &feature_list);
  testing::ClearAllVariationIDs();
  return base::FieldTrialList::FindFullName(kStudyName);
}

// Process the seed and return which group the user is assigned for Uniformity.
std::vector<std::string> GetUniformityAssignments(
    const VariationsSeed& seed,
    bool enable_benchmarking = false) {
  std::vector<std::string> result;
  // Add 20 clients that do not have client IDs, 1 per low entropy value.
  for (uint32_t i = 0; i < kMaxEntropy; i++) {
    EntropyProviders providers("", {i, kMaxEntropy}, enable_benchmarking);
    result.push_back(GetUniformityAssignment(seed, providers));
  }
  // Add 100 clients that do have client IDs, 5 per low entropy value.
  for (uint32_t i = 0; i < kMaxEntropy * 5; i++) {
    auto high_entropy = base::StringPrintf("clientid_%02d", i);
    ValueInRange low_entropy = {i % kMaxEntropy, kMaxEntropy};
    EntropyProviders providers(high_entropy, low_entropy, enable_benchmarking);
    result.push_back(GetUniformityAssignment(seed, providers));
  }
  return result;
}

std::vector<std::string> Concat(
    std::initializer_list<const std::vector<std::string>*> vectors) {
  std::vector<std::string> result;
  for (auto* const vector : vectors) {
    result.insert(result.end(), vector->begin(), vector->end());
  }
  return result;
}

// Computes the Chi-Square statistic for |assignment_counts| assuming they
// follow a uniform distribution, where each entry has expected value
// |expected_value|.
//
// The Chi-Square statistic is defined as Sum((O-E)^2/E) where O is the observed
// value and E is the expected value.
double ComputeChiSquare(const std::map<std::string, size_t>& assignment_counts,
                        double expected_value) {
  double sum = 0;
  for (const auto& [key, value] : assignment_counts) {
    const double delta = value - expected_value;
    sum += (delta * delta) / expected_value;
  }
  return sum;
}

}  // namespace

// We should get the same assignments for clients that have no high entropy.
// This should be true for both types of layer entropy.
TEST(VariationsUniformityTest, UnlayeredDefaultEntropyStudy) {
  auto assignments = GetUniformityAssignments(
      LayerStudySeed({.layer_constrain_study = false}));

  std::vector<std::string> expected = Concat({
      // Low entropy clients assign based on low entropy.
      &kExpectedLowEntropyAssignments,
      // High entropy clients assign based on high entropy.
      // No exclusions by layer.
      &kExpectedHighEntropyStudyAssignments,
  });
  EXPECT_THAT(assignments, ::testing::ElementsAreArray(expected));
}

// We should get the same assignments for clients that have no high entropy.
// This should be true for both types of layer entropy.
TEST(VariationsUniformityTest, UnlayeredLowEntropyStudy) {
  auto assignments = GetUniformityAssignments(LayerStudySeed(
      {.layer_constrain_study = false, .force_low_entropy = true}));

  std::vector<std::string> expected = Concat({
      // Low entropy clients assign based on low entropy.
      &kExpectedLowEntropyAssignments,
      // High entropy clients assign based on low entropy.
      // No exclusions by layer.
      &kExpectedLowEntropyAssignments,
      &kExpectedLowEntropyAssignments,
      &kExpectedLowEntropyAssignments,
      &kExpectedLowEntropyAssignments,
      &kExpectedLowEntropyAssignments,
  });
  EXPECT_THAT(assignments, ::testing::ElementsAreArray(expected));
}

TEST(VariationsUniformityTest, LowEntropyLayerDefaultEntropyStudy) {
  auto assignments = GetUniformityAssignments(LayerStudySeed({}));

  std::vector<std::string> expected = Concat({
      // Low entropy clients assign based on remainder entropy.
      &kExpectedRemainderEntropyAssignments,
      // High entropy clients assign based on high entropy.
      // Exclusions by layer below.
      &kExpectedHighEntropyStudyAssignments,
  });
  // Some high entropy clients are excluded from the layer by low entropy.
  for (int i = 1; i < 6; i++) {
    for (int les_value : {1, 12}) {
      expected[i * kMaxEntropy + les_value] = "";
    }
  }

  EXPECT_THAT(assignments, ::testing::ElementsAreArray(expected));
}

TEST(VariationsUniformityTest, LowEntropyLayerLowEntropyStudy) {
  auto assignments =
      GetUniformityAssignments(LayerStudySeed({.force_low_entropy = true}));

  // Both high and low entropy clients should use remainder entropy.
  std::vector<std::string> expected = Concat({
      &kExpectedRemainderEntropyAssignments,
      &kExpectedRemainderEntropyAssignments,
      &kExpectedRemainderEntropyAssignments,
      &kExpectedRemainderEntropyAssignments,
      &kExpectedRemainderEntropyAssignments,
      &kExpectedRemainderEntropyAssignments,
  });
  EXPECT_THAT(assignments, ::testing::ElementsAreArray(expected));
}

// We should get the same assignments for clients that have no high entropy.
// This should be true for both types of layer entropy.
TEST(VariationsUniformityTest, DefaultEntropyLayerDefaultEntropyStudy) {
  auto assignments = GetUniformityAssignments(
      LayerStudySeed({.force_low_entropy_layer = false}));

  std::vector<std::string> expected = Concat({
      // Low entropy clients assign based on remainder entropy.
      &kExpectedRemainderEntropyAssignments,
      // High entropy clients assign based on high entropy.
      // Exclusions by layer below.
      &kExpectedHighEntropyStudyAssignments,
  });
  // The following clients are excluded from the layer by high entropy.
  // This is derived from a sample of the high entropy space, and 18/100
  // exclusions is a likely (p>0.01) result at 10% exclusions.
  int exclusions[] = {
      1,  6,  7,  10, 17,  // 5
      20, 25, 30, 31, 32,  // 10
      38, 40, 62, 66, 67,  // 15
      69, 71, 77           // 18
  };
  for (int exclusion : exclusions) {
    expected[kMaxEntropy + exclusion] = "";
  }

  EXPECT_THAT(assignments, ::testing::ElementsAreArray(expected));
}

// Not specifying the `salt` field for the layer should fall back to using the
// `id` field as salt. Different salt values should result in different layer
// exclusions.
TEST(VariationsUniformityTest, LayerSalt) {
  auto assignments = GetUniformityAssignments(
      LayerStudySeed({.force_low_entropy_layer = false}));

  auto assignments_salt_not_specified = GetUniformityAssignments(
      LayerStudySeed({.force_low_entropy_layer = false, .salt = 0}));

  auto assignments_alternative_salt = GetUniformityAssignments(LayerStudySeed(
      {.force_low_entropy_layer = false, .salt = kLayerSalt + 1}));

  EXPECT_EQ(assignments, assignments_salt_not_specified);
  EXPECT_NE(assignments, assignments_alternative_salt);
}

// When enable_benchmarking is passed, layered studies should never activate.
TEST(VariationsUniformityTest, BenchmarkingDisablesLayeredStudies) {
  std::vector<std::string> expected(
      kExpectedLowEntropyAssignments.size() +
          kExpectedHighEntropyStudyAssignments.size(),
      "");
  EXPECT_THAT(GetUniformityAssignments(LayerStudySeed({}),
                                       /*enable_benchmarking*/ true),
              ::testing::ElementsAreArray(expected));
  EXPECT_THAT(
      GetUniformityAssignments(LayerStudySeed({.force_low_entropy = true}),
                               /*enable_benchmarking*/ true),
      ::testing::ElementsAreArray(expected));
  EXPECT_THAT(GetUniformityAssignments(
                  LayerStudySeed({.force_low_entropy_layer = false}),
                  /*enable_benchmarking*/ true),
              ::testing::ElementsAreArray(expected));
}

TEST(VariationsUniformityTest, SessionEntropyStudyChiSquare) {
  // Number of buckets in the simulated field trials.
  const size_t kBucketCount = 20;
  // Max number of iterations to perform before giving up and failing.
  const size_t kMaxIterationCount = 10000;
  // The number of iterations to perform before each time the statistical
  // significance of the results is checked.
  const size_t kCheckIterationCount = 1000;
  // This is the Chi-Square threshold from the Chi-Square statistic table for
  // 19 degrees of freedom (based on |kBucketCount|) with a 99.9% confidence
  // level. See: http://www.medcalc.org/manual/chi-square-table.php
  const double kChiSquareThreshold = 43.82;

  std::map<std::string, size_t> assignment_counts;

  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name(kStudyName);
  study->set_consistency(Study_Consistency_SESSION);
  for (size_t i = 0; i < kBucketCount; i++) {
    auto* experiment = study->add_experiment();
    const std::string name =
        base::StringPrintf("group%02d", static_cast<int>(i));
    experiment->set_name(name);
    experiment->set_probability_weight(1);
    assignment_counts[name] = 0;
  }

  // The persistent entropy shouldn't matter here.
  EntropyProviders entropy_providers("not_used", {0, 8000});

  for (size_t i = 1; i <= kMaxIterationCount; i += kCheckIterationCount) {
    for (size_t j = 0; j < kCheckIterationCount; j++) {
      assignment_counts[GetUniformityAssignment(seed, entropy_providers)]++;
    }
    // Only the configured groups should have been selected.
    EXPECT_EQ(assignment_counts.size(), kBucketCount);

    const double expected_value_per_bucket =
        static_cast<double>(i) / kBucketCount;
    const double chi_square =
        ComputeChiSquare(assignment_counts, expected_value_per_bucket);
    if (chi_square < kChiSquareThreshold)
      break;

    // If |i == kMaxIterationCount|, the Chi-Square statistic did not
    // converge after |kMaxIterationCount|.
    EXPECT_LT(i, kMaxIterationCount)
        << "Failed with chi_square = " << chi_square << " after "
        << kMaxIterationCount << " iterations.";
  }
}

}  // namespace variations
