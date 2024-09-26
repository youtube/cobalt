// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"

#include <array>

#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform {

namespace {

using proto::SegmentId;

// Default parameters for Chrome Start model.
constexpr SegmentId kChromeStartSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
constexpr int64_t kChromeStartSignalStorageLength = 28;
constexpr int64_t kChromeStartMinSignalCollectionLength = 28;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 1> kChromeStartUMAFeatures = {
    MetadataWriter::UMAFeature{
        .signal_type = proto::SignalType::HISTOGRAM_VALUE,
        .name = "Session.TotalDuration",
        .bucket_count = 28,
        .tensor_length = 28,
        .aggregation = proto::Aggregation::BUCKETED_COUNT,
        .enum_ids_size = 0}};

}  // namespace

LowUserEngagementModel::LowUserEngagementModel()
    : ModelProvider(kChromeStartSegmentId) {}

std::unique_ptr<Config> LowUserEngagementModel::GetConfig() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeLowUserEngagementSegmentationKey;
  config->segmentation_uma_name = kChromeLowUserEngagementUmaName;
  config->AddSegmentId(kChromeStartSegmentId,
                       std::make_unique<LowUserEngagementModel>());

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kSegmentationPlatformLowEngagementFeature,
      kVariationsParamNameSegmentSelectionTTLDays, 7);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kSegmentationPlatformLowEngagementFeature,
      kVariationsParamNameUnknownSelectionTTLDays, 7);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);
  config->is_boolean_segment = true;

  return config;
}

void LowUserEngagementModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata chrome_start_metadata;
  MetadataWriter writer(&chrome_start_metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kChromeStartMinSignalCollectionLength, kChromeStartSignalStorageLength);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMapping(
      kChromeLowUserEngagementSegmentationKey);

  // Set features.
  writer.AddUmaFeatures(kChromeStartUMAFeatures.data(),
                        kChromeStartUMAFeatures.size());

  constexpr int kModelVersion = 1;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindRepeating(model_updated_callback, kChromeStartSegmentId,
                          std::move(chrome_start_metadata), kModelVersion));
}

void LowUserEngagementModel::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != 28) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  float result = 0;
  bool weeks[4]{};
  for (unsigned i = 0; i < 28; ++i) {
    int week_idx = i / 7;
    weeks[week_idx] = weeks[week_idx] || inputs[i];
  }
  if (!weeks[0] || !weeks[1] || !weeks[2] || !weeks[3])
    result = 1;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}

bool LowUserEngagementModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
