// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_

#include "base/memory/stack_allocated.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"

namespace segmentation_platform {

inline constexpr std::array<float, 1> kZeroDefaultValue{0};

#define DEFINE_UMA_FEATURE_LATEST(var_name, uma_name)          \
  FeatureQuery var_name = FeatureQuery::FromUMAFeature(        \
      MetadataWriter::UMAFeature::FromValueHistogram(          \
          uma_name, 28, proto::Aggregation::LATEST_OR_DEFAULT, \
          kZeroDefaultValue.size(), kZeroDefaultValue.data()))

#define DEFINE_UMA_FEATURE_COUNT(var_name, uma_name, days) \
  FeatureQuery var_name = FeatureQuery::FromUMAFeature(    \
      MetadataWriter::UMAFeature::FromValueHistogram(      \
          uma_name, days, proto::Aggregation::COUNT))

#define DEFINE_UMA_FEATURE_ENUM_COUNT(var_name, uma_name, enum_id, enum_size, \
                                      days)                                   \
  FeatureQuery var_name = FeatureQuery::FromUMAFeature(                       \
      MetadataWriter::UMAFeature::FromEnumHistogram(uma_name, days, enum_id,  \
                                                    enum_size))

#define DEFINE_UMA_FEATURE_SUM(var_name, uma_name, days)             \
  FeatureQuery var_name = FeatureQuery::FromUMAFeature(              \
      MetadataWriter::UMAFeature::FromValueHistogram(uma_name, days, \
                                                     proto::Aggregation::SUM))

#define DEFINE_INPUT_CONTEXT(var_name, input_name)                          \
  constexpr std::array<MetadataWriter::CustomInput::Arg, 1> kArg##__LINE__{ \
      std::make_pair("name", input_name)};                                  \
  FeatureQuery var_name =                                                   \
      FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{            \
          .tensor_length = 1,                                               \
          .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,       \
          .name = input_name,                                               \
          .arg = kArg##__LINE__.data(),                                     \
          .arg_size = kArg##__LINE__.size()})

struct FeatureQuery {
  STACK_ALLOCATED();

 public:
  static constexpr FeatureQuery FromUMAFeature(
      MetadataWriter::UMAFeature uma_feature) {
    return FeatureQuery{.uma_feature = uma_feature};
  }
  static constexpr FeatureQuery FromSqlFeature(
      MetadataWriter::SqlFeature sql_feature) {
    return FeatureQuery{.sql_feature = sql_feature};
  }
  static constexpr FeatureQuery FromCustomInput(
      MetadataWriter::CustomInput custom_input) {
    return FeatureQuery{.custom_input = custom_input};
  }

  const std::optional<MetadataWriter::UMAFeature> uma_feature;
  const std::optional<MetadataWriter::SqlFeature> sql_feature;
  const std::optional<MetadataWriter::CustomInput> custom_input;
};

// Helper function to create a `FeatureQuery` from a custom input name.
constexpr FeatureQuery CreateFeatureQueryFromCustomInputName(
    const char* input_name) {
  return FeatureQuery::FromCustomInput(MetadataWriter::CustomInput{
      .tensor_length = 1,
      .fill_policy = proto::CustomInput::FILL_FROM_INPUT_CONTEXT,
      .name = input_name});
}

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_FEATURE_QUERY_H_
