// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
using proto::SignalType;
namespace stats {

TEST(StatsTest, ModelExecutionZeroValuePercent) {
  base::HistogramTester tester;
  std::vector<float> empty{};
  std::vector<float> single_zero{0};
  std::vector<float> single_non_zero{1};
  std::vector<float> all_zeroes{0, 0, 0};
  std::vector<float> one_non_zero{0, 2, 0};
  std::vector<float> all_non_zero{1, 2, 3};

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, empty);
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, single_zero);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 100));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, single_non_zero);
  EXPECT_EQ(
      2, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, all_zeroes);
  EXPECT_EQ(
      2,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 100));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, one_non_zero);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 66));

  RecordModelExecutionZeroValuePercent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, all_non_zero);
  EXPECT_EQ(
      3, tester.GetBucketCount(
             "SegmentationPlatform.ModelExecution.ZeroValuePercent.NewTab", 0));
}

TEST(StatsTest, AdaptiveToolbarSegmentSwitch) {
  std::string histogram("SegmentationPlatform.AdaptiveToolbar.SegmentSwitched");
  base::HistogramTester tester;
  Config config;
  config.segmentation_key = kAdaptiveToolbarSegmentationKey;
  config.segmentation_uma_name = kAdaptiveToolbarUmaName;
  config.auto_execute_and_cache = true;

  // Share -> New tab.
  RecordSegmentSelectionComputed(
      config, SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);

  // None -> Share.
  RecordSegmentSelectionComputed(
      config, SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, absl::nullopt);

  // Share -> Share.
  RecordSegmentSelectionComputed(
      config, SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kNoneToShare), 1),
          base::Bucket(
              static_cast<int>(AdaptiveToolbarSegmentSwitch::kShareToNewTab),
              1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.AdaptiveToolbar.SegmentSelection.Computed", 3);
}

TEST(StatsTest, SegmentSwitchWithMultiOutput) {
  std::string switched_histogram(
      "SegmentationPlatform.PowerUser.PostProcessing.TopLabel.Switched");

  base::HistogramTester tester;
  Config config;
  config.segmentation_key = kPowerUserKey;
  config.segmentation_uma_name = kPowerUserUmaName;
  config.auto_execute_and_cache = true;

  auto result_low = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.2},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  auto result_medium = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.3},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  auto result_high = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.8},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  auto result_underflow = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.1},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  // Low -> Low. No switched histograms.
  RecordClassificationResultUpdated(config, result_low, result_low);
  EXPECT_THAT(tester.GetAllSamples(switched_histogram), testing::ElementsAre());

  // Verify all possible combinations in a 3-label classifier.
  // none -> label 0 : -200
  // none -> label 1 : -199
  // none -> label 2 : -198
  // label 0 -> label 1 : 1
  // label 0 -> label 2 : 2
  // label 1 -> label 0 : 100
  // label 1 -> label 2 : 102
  // label 2 -> label 0 : 200
  // label 2 -> label 1 : 201
  RecordClassificationResultUpdated(config, absl::nullopt, result_low);
  RecordClassificationResultUpdated(config, absl::nullopt, result_medium);
  RecordClassificationResultUpdated(config, absl::nullopt, result_high);
  RecordClassificationResultUpdated(config, result_low, result_medium);
  RecordClassificationResultUpdated(config, result_low, result_high);
  RecordClassificationResultUpdated(config, result_medium, result_low);
  RecordClassificationResultUpdated(config, result_medium, result_high);
  RecordClassificationResultUpdated(config, result_high, result_low);
  RecordClassificationResultUpdated(config, result_high, result_medium);

  tester.ExpectTotalCount(switched_histogram, 9);
  EXPECT_THAT(tester.GetAllSamples(switched_histogram),
              testing::ElementsAre(base::Bucket(-200, 1), base::Bucket(-199, 1),
                                   base::Bucket(-198, 1), base::Bucket(1, 1),
                                   base::Bucket(2, 1), base::Bucket(100, 1),
                                   base::Bucket(102, 1), base::Bucket(200, 1),
                                   base::Bucket(201, 1)));
}

TEST(StatsTest, SegmentComputedWithMultiOutput) {
  std::string computed_histogram(
      "SegmentationPlatform.PowerUser.PostProcessing.TopLabel.Computed");

  base::HistogramTester tester;
  Config config;
  config.segmentation_key = kPowerUserKey;
  config.segmentation_uma_name = kPowerUserUmaName;
  config.auto_execute_and_cache = true;

  auto result_low = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.2},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  auto result_medium = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.3},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  auto result_high = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.8},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  auto result_underflow = metadata_utils::CreatePredictionResult(
      /*model_scores=*/{0.1},
      test_utils::GetTestOutputConfigForBinnedClassifier(),
      /*timestamp=*/base::Time::Now(), /*model_version=*/1);

  RecordClassificationResultComputed(config, result_low);
  RecordClassificationResultComputed(config, result_medium);
  RecordClassificationResultComputed(config, result_high);

  tester.ExpectTotalCount(computed_histogram, 3);
  EXPECT_THAT(tester.GetAllSamples(computed_histogram),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1),
                                   base::Bucket(2, 1)));
}

TEST(StatsTest, BooleanSegmentSwitch) {
  std::string histogram(
      "SegmentationPlatform.ChromeStartAndroidV2.SegmentSwitched");
  base::HistogramTester tester;
  Config config;
  config.segmentation_key = kChromeStartAndroidV2SegmentationKey;
  config.segmentation_uma_name = kChromeStartAndroidV2UmaName;
  config.auto_execute_and_cache = true;
  config.is_boolean_segment = true;

  // Start to none.
  RecordSegmentSelectionComputed(
      config, SegmentId::OPTIMIZATION_TARGET_UNKNOWN,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2);

  tester.ExpectTotalCount(histogram, 1);
  EXPECT_THAT(tester.GetAllSamples(histogram),
              testing::ElementsAre(base::Bucket(
                  static_cast<int>(BooleanSegmentSwitch::kEnabledToNone), 1)));
  // None to start.
  RecordSegmentSelectionComputed(
      config,
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2,
      absl::nullopt);

  tester.ExpectTotalCount(histogram, 2);

  EXPECT_THAT(
      tester.GetAllSamples(histogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(BooleanSegmentSwitch::kNoneToEnabled),
                       1),
          base::Bucket(static_cast<int>(BooleanSegmentSwitch::kEnabledToNone),
                       1)));
  tester.ExpectTotalCount(
      "SegmentationPlatform.ChromeStartAndroidV2.SegmentSelection.Computed2",
      2);
}

TEST(StatsTest, SignalsListeningCount) {
  base::HistogramTester tester;
  std::set<uint64_t> user_actions{1, 2, 3, 4};
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  histograms.insert(std::make_pair("hist1", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist2", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist3", SignalType::HISTOGRAM_ENUM));
  histograms.insert(std::make_pair("hist4", SignalType::HISTOGRAM_VALUE));
  histograms.insert(std::make_pair("hist5", SignalType::HISTOGRAM_VALUE));

  RecordSignalsListeningCount(user_actions, histograms);

  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.Signals.ListeningCount.UserAction", 4));
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramEnum", 3));
  EXPECT_EQ(
      1, tester.GetBucketCount(
             "SegmentationPlatform.Signals.ListeningCount.HistogramValue", 2));
}

TEST(StatsTest, TrainingDataCollectionEvent) {
  base::HistogramTester tester;
  RecordTrainingDataCollectionEvent(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      TrainingDataCollectionEvent::kImmediateCollectionStart);
  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.TrainingDataCollectionEvents.Share", 0));
}

TEST(StatsTest, RecordModelExecutionResult) {
  base::HistogramTester tester;

  // Test default case of multiplying result by 100.
  stats::RecordModelExecutionResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES, 0.19,
      proto::SegmentationModelMetadata::RETURN_TYPE_PROBABILITY);
  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.ModelExecution.Result.QueryTiles", 19));

  // Test segments that uses rank as scores, which should be recorded as-is.
  stats::RecordModelExecutionResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER, 75,
      proto::SegmentationModelMetadata::RETURN_TYPE_MULTISEGMENT);
  EXPECT_EQ(1,
            tester.GetBucketCount(
                "SegmentationPlatform.ModelExecution.Result.SearchUser", 75));

  // Test segments that returns an unbound float result, which should be
  // recorded as int.
  stats::RecordModelExecutionResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, 75.6,
      proto::SegmentationModelMetadata::RETURN_TYPE_INTEGER);
  EXPECT_EQ(1, tester.GetBucketCount(
                   "SegmentationPlatform.ModelExecution.Result.Share", 75));
}

TEST(StatsTest, RecordModelExecutionResultForMultiOutput) {
  base::HistogramTester tester;
  auto output_config = test_utils::GetTestOutputConfigForMultiClassClassifier(
      /*top_k-outputs=*/2,
      /*threshold=*/0.8);

  // Multi-class classifier should be recorded with results multiplied by 100.
  stats::RecordModelExecutionResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR,
      {0.4, 0.9, 0.15}, output_config);
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.Result.0.AdaptiveToolbar", 40));
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.Result.1.AdaptiveToolbar", 90));
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.Result.2.AdaptiveToolbar", 15));

  // Binned classifier is recorded as is.
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinnedClassifier({{0.2, "low"}, {0.3, "high"}},
                                            "none");
  stats::RecordModelExecutionResult(SegmentId::POWER_USER_SEGMENT, {5},
                                    model_metadata.output_config());
  EXPECT_EQ(
      1,
      tester.GetBucketCount(
          "SegmentationPlatform.ModelExecution.Result.0.PowerUserSegment", 5));
}

TEST(StatsTest, SegmentIdToHistogramVariant) {
  EXPECT_EQ("CrossDeviceUserSegment",
            SegmentIdToHistogramVariant(SegmentId::CROSS_DEVICE_USER_SEGMENT));
  EXPECT_EQ("NewTab", SegmentIdToHistogramVariant(
                          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB));
  EXPECT_EQ(
      "ChromeStartAndroidV2",
      SegmentIdToHistogramVariant(
          SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2));
  EXPECT_EQ("WebAppInstallationPromo",
            SegmentIdToHistogramVariant(
                SegmentId::OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO));
  EXPECT_EQ("Other", SegmentIdToHistogramVariant(
                         proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN));
}

}  // namespace stats
}  // namespace segmentation_platform
