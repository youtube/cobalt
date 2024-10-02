// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_util.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

#if BUILDFLAG(IS_WIN)
const char kOtherAbsoluteFilePath[] = "C:\\other\\absolute\\file\\path";
#else
const char kOtherAbsoluteFilePath[] = "/other/abs/file/path";
#endif

proto::ModelCacheKey CreateModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

}  // namespace

TEST(ModelUtilTest, GetModelOverrideForOptimizationTargetSwitchNotSet) {
  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
  EXPECT_FALSE(switches::IsModelOverridePresent());
}

TEST(ModelUtilTest, GetModelOverrideForOptimizationTargetEmptyInput) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kModelOverride);

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(ModelUtilTest, GetModelOverrideForOptimizationTargetBadInput) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride, "whatever");

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(ModelUtilTest,
     GetModelOverrideForOptimizationTargetInvalidOptimizationTarget) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      "notanoptimizationtarget:" + std::string(kTestAbsoluteFilePath));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(ModelUtilTest, GetModelOverrideForOptimizationTargetRelativeFilePath) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride, "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:" +
                                    std::string(kTestRelativeFilePath));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(ModelUtilTest,
     GetModelOverrideForOptimizationTargetRelativeFilePathWithMetadata) {
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestRelativeFilePath, encoded_metadata.c_str()));

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  EXPECT_EQ(absl::nullopt, file_path_and_metadata);
}

TEST(ModelUtilTest, GetModelOverrideForOptimizationTargetOneFilePath) {
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);
#if BUILDFLAG(IS_WIN)
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD|%s|%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));
#else
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s:%s",
                         kTestAbsoluteFilePath, encoded_metadata.c_str()));
#endif

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  ASSERT_TRUE(file_path_and_metadata);
  EXPECT_EQ(kTestAbsoluteFilePath, file_path_and_metadata->first);
  EXPECT_EQ("sometypeurl", file_path_and_metadata->second->type_url());
}

TEST(ModelUtilTest, GetModelOverrideForOptimizationTargetMultipleFilePath) {
  optimization_guide::proto::Any metadata;
  metadata.set_type_url("sometypeurl");
  std::string encoded_metadata;
  metadata.SerializeToString(&encoded_metadata);
  base::Base64Encode(encoded_metadata, &encoded_metadata);
#if BUILDFLAG(IS_WIN)
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD|%s,"
                         "OPTIMIZATION_TARGET_PAGE_TOPICS|%s|%s",
                         kTestAbsoluteFilePath, kOtherAbsoluteFilePath,
                         encoded_metadata.c_str()));
#else
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kModelOverride,
      base::StringPrintf("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD:%s,"
                         "OPTIMIZATION_TARGET_PAGE_TOPICS:%s:%s",
                         kTestAbsoluteFilePath, kOtherAbsoluteFilePath,
                         encoded_metadata.c_str()));
#endif

  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      file_path_and_metadata = GetModelOverrideForOptimizationTarget(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  ASSERT_TRUE(file_path_and_metadata);
  EXPECT_EQ(kTestAbsoluteFilePath, file_path_and_metadata->first);

  file_path_and_metadata = GetModelOverrideForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_TOPICS);
  ASSERT_TRUE(file_path_and_metadata);
  EXPECT_EQ(kOtherAbsoluteFilePath, file_path_and_metadata->first);
  EXPECT_EQ("sometypeurl", file_path_and_metadata->second->type_url());
}

TEST(ModelUtilTest, ModelCacheKeyHash) {
  EXPECT_EQ(GetModelCacheKeyHash(CreateModelCacheKey("en-US")),
            GetModelCacheKeyHash(CreateModelCacheKey("en-US")));
  EXPECT_NE(GetModelCacheKeyHash(CreateModelCacheKey("en-US")),
            GetModelCacheKeyHash(CreateModelCacheKey("en-UK")));
  EXPECT_TRUE(
      base::ranges::all_of(GetModelCacheKeyHash(CreateModelCacheKey("en-US")),
                           [](char ch) { return base::IsHexDigit(ch); }));
}

}  // namespace optimization_guide