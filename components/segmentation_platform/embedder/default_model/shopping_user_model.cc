// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for shopping user model.
constexpr SegmentId kShoppingUserSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER;
constexpr int64_t kShoppingUserSignalStorageLength = 28;
constexpr int64_t kShoppingUserMinSignalCollectionLength = 1;
constexpr int64_t kModelVersion = 1;
constexpr int kShoppingUserDefaultSelectionTTLDays = 7;
constexpr int kShoppingUserDefaultUnknownSelectionTTLDays = 7;

// InputFeatures.

constexpr std::array<int32_t, 1> kProductDetailAvailableEnums{1};

constexpr std::array<MetadataWriter::UMAFeature, 2> kShoppingUserUMAFeatures = {
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Commerce.PriceDrops.ActiveTabNavigationComplete.IsProductDetailPage",
        7,
        kProductDetailAvailableEnums.data(),
        kProductDetailAvailableEnums.size()),
    MetadataWriter::UMAFeature::FromUserAction(
        "Autofill_PolledCreditCardSuggestions",
        7)};
}  // namespace

// static
std::unique_ptr<Config> ShoppingUserModel::GetConfig() {
  if (!base::FeatureList::IsEnabled(features::kShoppingUserSegmentFeature)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kShoppingUserSegmentationKey;
  config->segmentation_uma_name = kShoppingUserUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER,
      std::make_unique<ShoppingUserModel>());
  config->segment_selection_ttl =
      base::Days(kShoppingUserDefaultSelectionTTLDays);
  config->unknown_selection_ttl =
      base::Days(kShoppingUserDefaultUnknownSelectionTTLDays);
  config->is_boolean_segment = true;
  return config;
}

ShoppingUserModel::ShoppingUserModel()
    : ModelProvider(kShoppingUserSegmentId) {}

void ShoppingUserModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata shopping_user_metadata;
  MetadataWriter writer(&shopping_user_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kShoppingUserMinSignalCollectionLength, kShoppingUserSignalStorageLength);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMapping(kShoppingUserSegmentationKey);

  // Set features.
  writer.AddUmaFeatures(kShoppingUserUMAFeatures.data(),
                        kShoppingUserUMAFeatures.size());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kShoppingUserSegmentId,
                          std::move(shopping_user_metadata), kModelVersion));
}

void ShoppingUserModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kShoppingUserUMAFeatures.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  float result = 0;

  // Determine if the user is a shopping user using  price tracking, price drop,
  // product detail page info, etc. features count.
  if (inputs[0] > 1 || inputs[1] > 1) {
    result = 1;  // User classified as shopping user;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

bool ShoppingUserModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
