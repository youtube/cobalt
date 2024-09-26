// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/database_maintenance_impl.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/metrics_hashes.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::SetArgReferee;

namespace segmentation_platform {

using SegmentId = proto::SegmentId;
using SignalType = proto::SignalType;
using Aggregation = proto::Aggregation;
using SignalIdentifier = std::pair<uint64_t, SignalType>;
using CleanupItem = std::tuple<uint64_t, SignalType, base::Time>;

namespace {
constexpr uint64_t kLatestCompactionDaysAgo = 2;
constexpr uint64_t kEarliestCompactionDaysAgo = 60;

std::string kTestSegmentationKey = "some_key";

struct SignalData {
  SegmentId target;
  proto::SignalType signal_type;
  std::string name;
  uint64_t name_hash;
  uint64_t bucket_count;
  uint64_t tensor_length;
  Aggregation aggregation;
  base::Time earliest_needed_timestamp;
  bool success;
};

}  // namespace

// Noop version. For database calls, just passes the calls to the DB.
// TODO(shaktisahu): Move this class to its own file.
class TestDefaultModelManager : public DefaultModelManager {
 public:
  TestDefaultModelManager()
      : DefaultModelManager(nullptr, base::flat_set<SegmentId>()) {}
  ~TestDefaultModelManager() override = default;

  void GetAllSegmentInfoFromDefaultModel(
      const base::flat_set<SegmentId>& segment_ids,
      MultipleSegmentInfoCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  DefaultModelManager::SegmentInfoList()));
  }

  void GetAllSegmentInfoFromBothModels(
      const base::flat_set<SegmentId>& segment_ids,
      SegmentInfoDatabase* segment_database,
      MultipleSegmentInfoCallback callback) override {
    segment_database->GetSegmentInfoForSegments(
        segment_ids,
        base::BindOnce(
            [](DefaultModelManager::MultipleSegmentInfoCallback callback,
               std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> db_list) {
              DefaultModelManager::SegmentInfoList list;
              for (auto& pair : *db_list) {
                list.push_back(std::make_unique<
                               DefaultModelManager::SegmentInfoWrapper>());
                list.back()->segment_source =
                    DefaultModelManager::SegmentSource::DATABASE;
                list.back()->segment_info.Swap(&pair.second);
              }
              std::move(callback).Run(std::move(list));
            },
            std::move(callback)));
  }
};

std::set<DatabaseMaintenanceImpl::SignalIdentifier> GetSignalIds(
    std::vector<SignalData> signal_datas) {
  std::set<DatabaseMaintenanceImpl::SignalIdentifier> signal_ids;
  for (auto& sd : signal_datas)
    signal_ids.emplace(sd.name_hash, sd.signal_type);

  return signal_ids;
}

std::vector<CleanupItem> GetCleanupItems(std::vector<SignalData> signal_datas) {
  std::vector<CleanupItem> cleanup_items;
  for (auto& sd : signal_datas) {
    cleanup_items.emplace_back(sd.name_hash, sd.signal_type,
                               sd.earliest_needed_timestamp);
  }
  return cleanup_items;
}

class DatabaseMaintenanceImplTest : public testing::Test {
 public:
  DatabaseMaintenanceImplTest() = default;
  ~DatabaseMaintenanceImplTest() override = default;

  void SetUp() override {
    SegmentationPlatformService::RegisterProfilePrefs(prefs_.registry());
    segment_info_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    signal_database_ = std::make_unique<MockSignalDatabase>();
    signal_storage_config_ = std::make_unique<MockSignalStorageConfig>();
    base::flat_set<SegmentId> segment_ids = {
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE};
    default_model_manager_ = std::make_unique<TestDefaultModelManager>();
    database_maintenance_ = std::make_unique<DatabaseMaintenanceImpl>(
        segment_ids, &clock_, segment_info_database_.get(),
        signal_database_.get(), signal_storage_config_.get(),
        default_model_manager_.get(), &prefs_);
    clock_.SetNow(base::Time::Now());
  }

  // Adds the provided features to the SegmentInfoDatabase.
  void AddFeatures(std::vector<SignalData> signal_datas) {
    for (auto& sd : signal_datas) {
      switch (sd.signal_type) {
        case SignalType::HISTOGRAM_VALUE: {
          segment_info_database_->AddHistogramValueFeature(
              sd.target, sd.name, sd.bucket_count, sd.tensor_length,
              sd.aggregation);
          break;
        }
        case SignalType::HISTOGRAM_ENUM: {
          segment_info_database_->AddHistogramEnumFeature(
              sd.target, sd.name, sd.bucket_count, sd.tensor_length,
              sd.aggregation, {});
          break;
        }
        case SignalType::USER_ACTION: {
          segment_info_database_->AddUserActionFeature(
              sd.target, sd.name, sd.bucket_count, sd.tensor_length,
              sd.aggregation);
          break;
        }
        default: {
          CHECK(false) << "Incorrect SignalType";
          break;
        }
      }
    }
  }

  void TestMaintenanceTasksScheduling(uint64_t earliest_days_ago) {
    std::vector<SignalData> signal_datas = {
        {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
         SignalType::HISTOGRAM_VALUE, "Foo", base::HashMetricName("Foo"), 44, 1,
         Aggregation::COUNT, clock_.Now() - base::Days(10), true},
        {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
         SignalType::HISTOGRAM_ENUM, "Bar", base::HashMetricName("Bar"), 33, 1,
         Aggregation::COUNT, clock_.Now() - base::Days(5), true},
        {SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
         SignalType::USER_ACTION, "Failed", base::HashMetricName("Failed"), 22,
         1, Aggregation::COUNT, clock_.Now() - base::Days(1), false},
    };

    // Prepare test setup.
    AddFeatures(signal_datas);
    std::set<DatabaseMaintenanceImpl::SignalIdentifier> signal_ids =
        GetSignalIds(signal_datas);
    std::vector<CleanupItem> cleanup_items = GetCleanupItems(signal_datas);

    // Ensure we return the correct signals.
    ON_CALL(*signal_storage_config_, GetSignalsForCleanup(_, _))
        .WillByDefault(SetArgReferee<1>(cleanup_items));

    // We should try to delete each signal.
    for (auto& sd : signal_datas) {
      EXPECT_CALL(*signal_database_,
                  DeleteSamples(sd.signal_type, sd.name_hash,
                                sd.earliest_needed_timestamp, _))
          .WillOnce(RunOnceCallback<3>(sd.success));
    }

    // The Failed signal failed to clean up, so we should not be updating it,
    // but the rest should be updated.
    cleanup_items.erase(
        std::remove_if(cleanup_items.begin(), cleanup_items.end(),
                       [](CleanupItem item) {
                         return std::get<0>(item) ==
                                base::HashMetricName("Failed");
                       }),
        cleanup_items.end());
    EXPECT_CALL(*signal_storage_config_,
                UpdateSignalsForCleanup(cleanup_items));

    // Verify that for each of the signal data, we get a compaction request for
    // each day within the correct range.
    for (auto& sd : signal_datas) {
      for (uint64_t days_ago = kLatestCompactionDaysAgo;
           days_ago <= earliest_days_ago; ++days_ago) {
        EXPECT_CALL(
            *signal_database_,
            CompactSamplesForDay(sd.signal_type, sd.name_hash,
                                 clock_.Now().UTCMidnight() + base::Days(1) -
                                     base::Seconds(1) - base::Days(days_ago),
                                 _))
            .WillOnce(RunOnceCallback<3>(/*success=*/sd.success));
      }
    }
  }
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<Config> config_;
  base::SimpleTestClock clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_info_database_;
  std::unique_ptr<MockSignalDatabase> signal_database_;
  std::unique_ptr<MockSignalStorageConfig> signal_storage_config_;
  std::unique_ptr<TestDefaultModelManager> default_model_manager_;

  std::unique_ptr<DatabaseMaintenanceImpl> database_maintenance_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(DatabaseMaintenanceImplTest, ExecuteMaintenanceTasks) {
  TestMaintenanceTasksScheduling(kEarliestCompactionDaysAgo);
  // Kick off all tasks.
  database_maintenance_->ExecuteMaintenanceTasks();
}

TEST_F(DatabaseMaintenanceImplTest, NoCompactionForDataAlreadyCompacted) {
  // Simulate that a maintenance task has compacted all samples 4 days ago.
  prefs_.SetTime(kSegmentationLastDBCompactionTimePref,
                 clock_.Now().UTCMidnight() - base::Days(3) - base::Seconds(1));
  TestMaintenanceTasksScheduling(3);
  // Kick off all tasks.
  database_maintenance_->ExecuteMaintenanceTasks();
}

}  // namespace segmentation_platform
