// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/flatbuffer_utils.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

// Helper builder configuration to produce highly tailored malformed test
// models.
struct ModelBuilderOptions {
  bool include_metadata = true;
  bool include_hashes = true;
  bool include_rules = true;
  bool include_page_terms = true;
  bool include_page_words = true;
  bool include_thresholds = true;

  size_t hash_count = 5;
  std::vector<std::vector<int32_t>> rules = {{0}, {0, 1}};
  std::vector<int32_t> page_terms = {3, 4};
  std::vector<uint32_t> page_words = {1000, 2000};
  bool include_threshold_label = true;
};

std::string BuildTestModel(const ModelBuilderOptions& options) {
  flatbuffers::FlatBufferBuilder builder(1024);

  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>>
      hashes_flat;
  if (options.include_hashes) {
    std::vector<flatbuffers::Offset<flat::Hash>> hashes;
    for (size_t i = 0; i < options.hash_count; ++i) {
      std::string feature = "feature" + base::NumberToString(i);
      std::vector<uint8_t> hash_data(feature.begin(), feature.end());
      hashes.push_back(flat::CreateHashDirect(builder, &hash_data));
    }
    hashes_flat = builder.CreateVector(hashes);
  }

  flatbuffers::Offset<
      flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>>
      rules_flat;
  if (options.include_rules) {
    std::vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>> rules;
    for (const auto& features : options.rules) {
      rules.push_back(
          flat::ClientSideModel_::CreateRuleDirect(builder, &features, 1.0));
    }
    rules_flat = builder.CreateVector(rules);
  }

  flatbuffers::Offset<flatbuffers::Vector<int32_t>> page_term_flat;
  if (options.include_page_terms) {
    page_term_flat = builder.CreateVector(options.page_terms);
  }

  flatbuffers::Offset<flatbuffers::Vector<uint32_t>> page_word_flat;
  if (options.include_page_words) {
    page_word_flat = builder.CreateVector(options.page_words);
  }

  flatbuffers::Offset<flat::TfLiteModelMetadata> tflite_metadata_flat;
  if (options.include_metadata) {
    flatbuffers::Offset<flatbuffers::Vector<
        flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>>
        thresholds_flat;
    if (options.include_thresholds) {
      std::vector<flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>
          thresholds_vector;
      flatbuffers::Offset<flatbuffers::String> label =
          options.include_threshold_label
              ? builder.CreateString("label")
              : flatbuffers::Offset<flatbuffers::String>();
      thresholds_vector.push_back(
          flat::TfLiteModelMetadata_::CreateThreshold(builder, label, 0.5));
      thresholds_flat = builder.CreateVector(thresholds_vector);
    }
    tflite_metadata_flat =
        flat::CreateTfLiteModelMetadata(builder, 0, thresholds_flat, 0, 0);
  }

  flat::ClientSideModelBuilder csd_model_builder(builder);
  if (options.include_hashes) {
    csd_model_builder.add_hashes(hashes_flat);
  }
  if (options.include_rules) {
    csd_model_builder.add_rule(rules_flat);
  }
  if (options.include_page_terms) {
    csd_model_builder.add_page_term(page_term_flat);
  }
  if (options.include_page_words) {
    csd_model_builder.add_page_word(page_word_flat);
  }
  if (options.include_metadata) {
    csd_model_builder.add_tflite_metadata(tflite_metadata_flat);
  }

  builder.Finish(csd_model_builder.Finish());
  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace

class FlatbufferUtilsTest : public ::testing::Test {};

TEST_F(FlatbufferUtilsTest, VerifyValidModelReturnsTrue) {
  ModelBuilderOptions options;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_TRUE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyMissingMetadataReturnsFalse) {
  ModelBuilderOptions options;
  options.include_metadata = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyMissingHashesReturnsFalse) {
  ModelBuilderOptions options;
  options.include_hashes = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyMissingRulesReturnsFalse) {
  ModelBuilderOptions options;
  options.include_rules = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyRuleFeatureOutOfBoundsReturnsFalse) {
  {
    ModelBuilderOptions options;
    // Feature index 5 is out of bounds (hash count is 5, indices are 0 to 4)
    options.rules = {{5}};
    std::string data = BuildTestModel(options);
    const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
    EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
  }
  {
    ModelBuilderOptions options;
    // Feature index -1 is out of bounds
    options.rules = {{-1}};
    std::string data = BuildTestModel(options);
    const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
    EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
  }
}

TEST_F(FlatbufferUtilsTest, VerifyMissingPageTermsReturnsFalse) {
  ModelBuilderOptions options;
  options.include_page_terms = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyPageTermOutOfBoundsReturnsFalse) {
  {
    ModelBuilderOptions options;
    // Page term index 5 is out of bounds (hash count is 5)
    options.page_terms = {5};
    std::string data = BuildTestModel(options);
    const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
    EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
  }
  {
    ModelBuilderOptions options;
    // Page term index -1 is out of bounds
    options.page_terms = {-1};
    std::string data = BuildTestModel(options);
    const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
    EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
  }
}

TEST_F(FlatbufferUtilsTest, VerifyMissingPageWordsReturnsFalse) {
  ModelBuilderOptions options;
  options.include_page_words = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyMissingThresholdsReturnsFalse) {
  ModelBuilderOptions options;
  options.include_thresholds = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

TEST_F(FlatbufferUtilsTest, VerifyThresholdMissingLabelReturnsFalse) {
  ModelBuilderOptions options;
  options.include_threshold_label = false;
  std::string data = BuildTestModel(options);
  const flat::ClientSideModel* model = flat::GetClientSideModel(data.data());
  EXPECT_FALSE(VerifyCSDFlatBufferIndicesAndFields(model));
}

}  // namespace safe_browsing
