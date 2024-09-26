// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

// Test Ids.
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

const TrainingRequestId kRequestId = TrainingRequestId::FromUnsafeValue(1);

}  // namespace

class TrainingDataCacheTest : public testing::Test {
 public:
  TrainingDataCacheTest() = default;
  ~TrainingDataCacheTest() override = default;

 protected:
  void SetUp() override {
    DCHECK(!training_data_cache_);
    test_segment_info_db_ = std::make_unique<test::TestSegmentInfoDatabase>();
    training_data_cache_ =
        std::make_unique<TrainingDataCache>(test_segment_info_db_.get());
  }

  void TearDown() override { training_data_cache_.reset(); }

  void VerifyGetInputsAndDelete(proto::SegmentId segment_id,
                                TrainingRequestId request_id,
                                absl::optional<proto::TrainingData> expected) {
    training_data_cache_->GetInputsAndDelete(
        kSegmentId, kRequestId,
        base::BindOnce(&TrainingDataCacheTest::OnGetTrainingData,
                       base::Unretained(this), expected));
  }

  void OnGetTrainingData(absl::optional<proto::TrainingData> expected,
                         absl::optional<proto::TrainingData> actual) {
    EXPECT_EQ(actual.has_value(), expected.has_value());
    if (expected.has_value()) {
      EXPECT_EQ(actual.value().inputs_size(), expected.value().inputs_size());
      for (int i = 0; i < expected.value().inputs_size(); i++) {
        EXPECT_EQ(actual.value().inputs(i), expected.value().inputs(i));
      }
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SegmentInfoDatabase> test_segment_info_db_;
  std::unique_ptr<TrainingDataCache> training_data_cache_;
};

TEST_F(TrainingDataCacheTest, GetTrainingDataFromEmptyCache) {
  // Empty cache should return no result.
  VerifyGetInputsAndDelete(kSegmentId, kRequestId, absl::nullopt);
}

TEST_F(TrainingDataCacheTest, GetTrainingDataFromCache) {
  proto::TrainingData training_data;
  training_data.add_inputs(1);
  training_data.set_request_id(kRequestId.GetUnsafeValue());

  // Store a training data request to cache.
  training_data_cache_->StoreInputs(kSegmentId, training_data,
                                    /*save_to_db*/ false);

  // Cache will return and delete the corresponding training data.
  VerifyGetInputsAndDelete(kSegmentId, kRequestId, training_data);

  // Cache/DB will return no result since the request has been deleted.
  VerifyGetInputsAndDelete(kSegmentId, kRequestId, absl::nullopt);
}

TEST_F(TrainingDataCacheTest, GetTrainingDataFromDB) {
  proto::TrainingData training_data;
  training_data.add_inputs(1);
  training_data.set_request_id(kRequestId.GetUnsafeValue());

  // Store a training data request to the DB.
  test_segment_info_db_->SaveTrainingData(kSegmentId, training_data,
                                          base::DoNothing());

  // DB will return and delete the corresponding training data.
  VerifyGetInputsAndDelete(kSegmentId, kRequestId, training_data);

  // Cache/DB will return no result since the request has been deleted.
  VerifyGetInputsAndDelete(kSegmentId, kRequestId, absl::nullopt);
}

}  // namespace segmentation_platform
