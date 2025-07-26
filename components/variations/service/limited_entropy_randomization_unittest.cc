// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/limited_entropy_randomization.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/limited_entropy_mode_gate.h"
#include "components/variations/proto/layer.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_layers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

constexpr int kTestLayerId = 101;
constexpr int kTestLayerMemberId = 201;
constexpr char kTestClientId[] = "test_client_id";

// Hard code the enum bucket values, and use these in tests so that any
// re-numbering can be detected.
// LimitedEntropySeedRejectionReason::kHighEntropyUsage
constexpr int kHighEntropyUsageBucket = 0;
// LimitedEntropySeedRejectionReason::kMoreThenOneLimitedLayer
constexpr int kMoreThenOneLimitedLayerBucket = 1;
// LimitedEntropySeedRejectionReason::kLimitedLayerHasInvalidSlotBounds
constexpr int kLimitedLayerHasInvalidSlotBoundsBucket = 2;
// LimitedEntropySeedRejectionReason::kLimitedLayerDoesNotContainSlots
constexpr int kLimitedLayerDoesNotContainSlotsBucket = 3;
// InvalidLayerReason::kNoSlots
constexpr int kNoSlotsBucket = 1;
// InvalidLayerReason::kInvalidSlotBounds
constexpr int kInvalidSlotBoundsBucket = 5;

// The following value ensures slot 0 is selected  (among 100 slots) when the
// limited entropy provider is used.
constexpr char kTestLimitedEntropyRandomizationSource[] =
    "limited_entropy_randomization_source_964";

Study::Experiment CreateExperiment(int weight) {
  Study::Experiment experiment;
  experiment.set_probability_weight(weight);
  return experiment;
}

Study::Experiment CreateGoogleWebExperiment(int weight,
                                            int google_web_experiment_id) {
  auto experiment = CreateExperiment(weight);
  experiment.set_google_web_experiment_id(google_web_experiment_id);
  return experiment;
}

std::vector<Study::Experiment> CreateExperimentsWithTwoBitsOfEntropy() {
  // Create 3 experiments with a total probability weight of 100. Only the 25%
  // group has a google_web_experiment_id so the entropy used should be
  // -log2(1/4) = 2.
  std::vector<Study::Experiment> experiments = {
      CreateExperiment(10), CreateGoogleWebExperiment(25, 100001),
      CreateExperiment(65)};
  return experiments;
}

LayerMemberReference CreateLayerMemberReference(
    uint32_t layer_id,
    const std::vector<uint32_t>& layer_member_ids) {
  LayerMemberReference layer_member_reference;
  layer_member_reference.set_layer_id(layer_id);
  for (uint32_t layer_member_id : layer_member_ids) {
    layer_member_reference.add_layer_member_ids(layer_member_id);
  }
  return layer_member_reference;
}

// Creates a test study.
Study CreateTestStudy(const std::vector<Study::Experiment>& experiments) {
  Study study;
  study.set_name("test_study");

  std::vector<Study::Experiment> copied_experiments(experiments);
  for (size_t i = 0; i < copied_experiments.size(); ++i) {
    copied_experiments[i].set_name(
        base::StrCat({"test_experiment_", base::NumberToString(i)}));
    Study_Experiment* experiment_to_add = study.add_experiment();
    experiment_to_add->MergeFrom(copied_experiments[i]);
  }

  return study;
}

Study CreateTestStudy(const std::vector<Study::Experiment>& experiments,
                      const LayerMemberReference& layer_member_reference) {
  auto study = CreateTestStudy(experiments);
  auto* layer_member_reference_to_add = study.mutable_layer();
  layer_member_reference_to_add->MergeFrom(layer_member_reference);
  return study;
}

// Creates a test layer member with slot ranges given as <start, end> pairs.
Layer::LayerMember CreateLayerMember(
    int layer_member_id,
    const std::vector<std::pair<int, int>>& slot_ranges) {
  Layer::LayerMember layer_member;
  layer_member.set_id(layer_member_id);

  for (const auto& slot_range : slot_ranges) {
    auto* slot = layer_member.add_slots();
    slot->set_start(slot_range.first);
    slot->set_end(slot_range.second);
  }
  return layer_member;
}

Layer CreateLayer(int layer_id,
                  int num_slots,
                  Layer::EntropyMode entropy_mode,
                  const std::vector<Layer::LayerMember>& layer_members) {
  Layer layer;
  layer.set_id(layer_id);
  layer.set_num_slots(num_slots);
  layer.set_entropy_mode(entropy_mode);

  for (const auto& layer_member : layer_members) {
    auto* member_to_add = layer.add_members();
    member_to_add->MergeFrom(layer_member);
  }

  return layer;
}

VariationsSeed CreateTestSeed(const std::vector<Layer>& layers,
                              const std::vector<Study>& studies) {
  VariationsSeed seed;

  for (const auto& study : studies) {
    auto* study_to_add = seed.add_study();
    study_to_add->MergeFrom(study);
  }

  for (const auto& layer : layers) {
    auto* layer_to_add = seed.add_layers();
    layer_to_add->MergeFrom(layer);
  }

  return seed;
}

}  // namespace

class LimitedEntropyRandomizationTest : public ::testing::Test {
 public:
  LimitedEntropyRandomizationTest()
      : entropy_providers_(
            kTestClientId,
            // Using 100 as the test LES value (as opposed to the production
            // value of 8000) since this test suite is for using the limited
            // entropy randomization source.
            {0, 100},
            kTestLimitedEntropyRandomizationSource) {
    EnableLimitedEntropyModeForTesting();
  }

 protected:
  base::HistogramTester histogram_tester_;
  const EntropyProviders entropy_providers_;
};

TEST_F(LimitedEntropyRandomizationTest,
       TestIsEnoughEntropyAvailable_NotEnoughEntropy) {
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId})),
                     CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy())});

  VariationsLayers layers(test_seed, entropy_providers_);
  // Entropy limit is 2 and entropy used is 3 (1 bit from the layer member and 2
  // bits from the studies assigned).
  EXPECT_FALSE(IsEnoughLimitedEntropyAvailableForTesting(layers, test_seed, 2));
}

TEST_F(LimitedEntropyRandomizationTest,
       TestIsEnoughEntropyAvailable_EntropyAvailable) {
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId})),
                     CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy())});

  VariationsLayers layers(test_seed, entropy_providers_);
  // Entropy limit is 3 and entropy used is 3 (1 bit from the layer member and 2
  // bits from the studies assigned).
  EXPECT_TRUE(IsEnoughLimitedEntropyAvailableForTesting(layers, test_seed, 3));
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_EntropyOveruse) {
  // Creates a layer with LIMITED entropy mode that takes 1 bit of entropy from
  // the layer member.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::LIMITED,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId}))});
  VariationsLayers layers(test_seed, entropy_providers_);

  // The total entropy used should be 3 bits which is over the limit.
  EXPECT_TRUE(SeedHasMisconfiguredEntropy(layers, test_seed));
  histogram_tester_.ExpectUniqueSample(
      "Variations.LimitedEntropy.SeedRejectionReason", kHighEntropyUsageBucket,
      1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_MultipleLimitedLayer) {
  std::vector<Layer> test_layers;
  for (int i = 0; i < 4; ++i) {
    test_layers.push_back(CreateLayer(
        /*layer_id=*/i, /*num_slots=*/100,
        /*entropy_mode=*/Layer::LIMITED,
        /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})}));
  }
  auto test_seed = CreateTestSeed(
      test_layers,
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(0, {kTestLayerMemberId}))});
  VariationsLayers layers(test_seed, entropy_providers_);

  EXPECT_TRUE(SeedHasMisconfiguredEntropy(layers, test_seed));
  histogram_tester_.ExpectUniqueSample(
      "Variations.LimitedEntropy.SeedRejectionReason",
      kMoreThenOneLimitedLayerBucket, kMoreThenOneLimitedLayerBucket);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_InvalidSlotBounds) {
  // A test layer with overlapping layer members.
  auto test_layer =
      CreateLayer(kTestLayerId, /*num_slots=*/100, Layer::LIMITED,
                  {CreateLayerMember(kTestLayerMemberId, {{0, 49}, {25, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer},
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(0, {kTestLayerMemberId}))});
  VariationsLayers layers(test_seed, entropy_providers_);

  // Seed should not be rejected since the LIMITED layer is invalid and no
  // constrained studies will be assigned.
  EXPECT_FALSE(SeedHasMisconfiguredEntropy(layers, test_seed));

  // There should only be one metric emission on the layer being invalid.
  // `kLimitedLayerHasInvalidSlotBounds` should only be emitted when the layer
  // is deemed valid, but still has invalid slot bounds.
  histogram_tester_.ExpectUniqueSample(
      "Variations.LimitedEntropy.SeedRejectionReason",
      kLimitedLayerHasInvalidSlotBoundsBucket, 0);
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       kInvalidSlotBoundsBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, SeedRejection_NoSlots) {
  // A test layer with no slots.
  auto test_layer = CreateLayer(kTestLayerId, /*num_slots=*/0, Layer::LIMITED,
                                {CreateLayerMember(kTestLayerMemberId, {})});
  auto test_seed = CreateTestSeed(
      {test_layer},
      {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                       CreateLayerMemberReference(0, {kTestLayerMemberId}))});
  VariationsLayers layers(test_seed, entropy_providers_);

  // Similar to SeedRejection_InvalidSlotBounds, the seed should not be rejected
  // since the LIMITED layer is invalid and no constrained studies will be
  // assigned.
  EXPECT_FALSE(SeedHasMisconfiguredEntropy(layers, test_seed));

  // Similar to SeedRejection_InvalidSlotBounds, there should only be one
  // metric emission on the layer having so slots.
  // `kLimitedLayerDoesNotContainSlots` should only be emitted when the
  // layer is deemed valid, but still has no slot.
  histogram_tester_.ExpectUniqueSample(
      "Variations.LimitedEntropy.SeedRejectionReason",
      kLimitedLayerDoesNotContainSlotsBucket, 0);
  histogram_tester_.ExpectUniqueSample("Variations.InvalidLayerReason",
                                       kNoSlotsBucket, 1);
}

TEST_F(LimitedEntropyRandomizationTest, DoNotRejectTheSeed_NonLimitedLayer) {
  // Creates a layer with DEFAULT entropy mode. It would have taken 1 bit of
  // entropy if it is using LIMITED entropy mode.
  auto test_layer = CreateLayer(
      /*layer_id=*/kTestLayerId, /*num_slots=*/100,
      /*entropy_mode=*/Layer::DEFAULT,
      /*layer_members=*/{CreateLayerMember(kTestLayerMemberId, {{0, 49}})});
  auto test_seed = CreateTestSeed(
      {test_layer}, {CreateTestStudy(CreateExperimentsWithTwoBitsOfEntropy(),
                                     CreateLayerMemberReference(
                                         kTestLayerId, {kTestLayerMemberId}))});
  VariationsLayers layers(test_seed, entropy_providers_);

  // Seed should not be rejected since it's not using LIMITED entropy mode.
  EXPECT_FALSE(SeedHasMisconfiguredEntropy(layers, test_seed));

  histogram_tester_.ExpectUniqueSample(
      "Variations.LimitedEntropy.SeedRejectionReason", kHighEntropyUsageBucket,
      0);
}

}  // namespace variations
