// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/core/tips_service.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/tips/core/tips_feature.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/testing/mock_database_client.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace tips {

namespace {

class MockTipsFeature : public TipsFeature {
 public:
  MockTipsFeature(TipFeatureRank rank,
                  TipsNotificationsFeatureType type,
                  std::vector<SignalDefinition> signals,
                  bool is_eligible)
      : rank_(rank),
        type_(type),
        signals_(std::move(signals)),
        is_eligible_(is_eligible) {}

  TipFeatureRank GetRank() const override { return rank_; }
  TipsNotificationsFeatureType GetFeatureType() const override { return type_; }
  std::vector<SignalDefinition> GetRequiredSignals() const override {
    return signals_;
  }
  bool IsEligible(const std::map<std::string, float>& signal_values,
                  const PrefService& pref_service) const override {
    last_signal_values_ = signal_values;
    return is_eligible_;
  }
  notifications::NotificationData GetNotificationData() const override {
    return notifications::NotificationData();
  }

  mutable std::map<std::string, float> last_signal_values_;

 private:
  TipFeatureRank rank_;
  TipsNotificationsFeatureType type_;
  std::vector<SignalDefinition> signals_;
  bool is_eligible_;
};

}  // namespace

class TipsServiceTest : public ::testing::Test {
 public:
  TipsServiceTest() = default;
  ~TipsServiceTest() override = default;

  void SetUp() override {
    service_ =
        std::make_unique<TipsService>(&pref_service_, &segmentation_service_);
  }

  void TearDown() override { service_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  segmentation_platform::MockSegmentationPlatformService segmentation_service_;
  segmentation_platform::MockDatabaseClient database_client_;
  std::unique_ptr<TipsService> service_;
};

TEST_F(TipsServiceTest, DetermineBestTip_NoDatabaseClient) {
  EXPECT_CALL(segmentation_service_, GetDatabaseClient())
      .WillOnce(Return(nullptr));

  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("SomeAction", 7)},
      /*is_eligible=*/true));

  std::optional<TipsNotificationsFeatureType> result;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         std::optional<TipsNotificationsFeatureType> res) { *out = res; },
      &result));
  EXPECT_FALSE(result.has_value());
}

TEST_F(TipsServiceTest, DetermineBestTip_SuccessfulEvaluation) {
  EXPECT_CALL(segmentation_service_, GetDatabaseClient())
      .WillRepeatedly(Return(&database_client_));

  auto feature = std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("SomeAction", 7)},
      /*is_eligible=*/true);
  MockTipsFeature* feature_ptr = feature.get();
  service_->RegisterFeature(std::move(feature));

  EXPECT_CALL(database_client_, ProcessFeatures(_, _, _))
      .WillOnce(
          [](const segmentation_platform::proto::SegmentationModelMetadata&,
             base::Time,
             segmentation_platform::DatabaseClient::FeaturesCallback callback) {
            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::vector<float>{42.0f});
          });

  std::optional<TipsNotificationsFeatureType> result;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         std::optional<TipsNotificationsFeatureType> res) { *out = res; },
      &result));

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, TipsNotificationsFeatureType::kQuickDelete);
  EXPECT_EQ(feature_ptr->last_signal_values_["SomeAction"], 42.0f);
}

TEST_F(TipsServiceTest, DetermineBestTip_DatabaseFailure) {
  EXPECT_CALL(segmentation_service_, GetDatabaseClient())
      .WillRepeatedly(Return(&database_client_));

  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kQuickDelete, TipsNotificationsFeatureType::kQuickDelete,
      std::vector<SignalDefinition>{UserAction("SomeAction", 7)},
      /*is_eligible=*/true));

  EXPECT_CALL(database_client_, ProcessFeatures(_, _, _))
      .WillOnce(
          [](const segmentation_platform::proto::SegmentationModelMetadata&,
             base::Time,
             segmentation_platform::DatabaseClient::FeaturesCallback callback) {
            std::move(callback).Run(ResultStatus::kError, std::vector<float>{});
          });

  std::optional<TipsNotificationsFeatureType> result;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         std::optional<TipsNotificationsFeatureType> res) { *out = res; },
      &result));

  EXPECT_FALSE(result.has_value());
}

TEST_F(TipsServiceTest, DetermineBestTip_RankingPriority) {
  EXPECT_CALL(segmentation_service_, GetDatabaseClient())
      .WillRepeatedly(Return(&database_client_));

  // Register lower priority feature first (kBottomOmnibox = 3)
  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kBottomOmnibox,
      TipsNotificationsFeatureType::kBottomOmnibox,
      std::vector<SignalDefinition>{HistogramSum("SignalA", 1)},
      /*is_eligible=*/true));

  // Register higher priority feature second (kGoogleLens = 2)
  service_->RegisterFeature(std::make_unique<MockTipsFeature>(
      TipFeatureRank::kGoogleLens, TipsNotificationsFeatureType::kGoogleLens,
      std::vector<SignalDefinition>{
          HistogramEnum("SignalB", 1, std::vector<int32_t>{1, 2})},
      /*is_eligible=*/true));

  EXPECT_CALL(database_client_, ProcessFeatures(_, _, _))
      .WillOnce(
          [](const segmentation_platform::proto::SegmentationModelMetadata&,
             base::Time,
             segmentation_platform::DatabaseClient::FeaturesCallback callback) {
            std::move(callback).Run(ResultStatus::kSuccess,
                                    std::vector<float>{1.0f, 2.0f});
          });

  std::optional<TipsNotificationsFeatureType> result;
  service_->DetermineBestTip(base::BindOnce(
      [](std::optional<TipsNotificationsFeatureType>* out,
         std::optional<TipsNotificationsFeatureType> res) { *out = res; },
      &result));

  EXPECT_TRUE(result.has_value());
  // Should select kGoogleLens because its rank (2) has higher display priority
  // than kBottomOmnibox (3)
  EXPECT_EQ(*result, TipsNotificationsFeatureType::kGoogleLens);
}

}  // namespace tips
