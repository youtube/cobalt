// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/ukm_data_manager_test_utils.h"

#include "base/run_loop.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/segmentation_platform/embedder/model_provider_factory_impl.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/internal/execution/model_manager_impl.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace segmentation_platform {

namespace {

using ::segmentation_platform::proto::SegmentId;
using ::testing::Return;
using ::ukm::builders::PageLoad;

// Returns a sample UKM entry.
ukm::mojom::UkmEntryPtr GetSamplePageLoadEntry(ukm::SourceId source_id) {
  ukm::mojom::UkmEntryPtr entry = ukm::mojom::UkmEntry::New();
  entry->source_id = source_id;
  entry->event_hash = PageLoad::kEntryNameHash;
  entry->metrics[PageLoad::kCpuTimeNameHash] = 10;
  entry->metrics[PageLoad::kIsNewBookmarkNameHash] = 20;
  entry->metrics[PageLoad::kIsNTPCustomLinkNameHash] = 30;
  return entry;
}

// Runs the given query and returns the result as float value. See
// RunReadonlyQueries() for more info.
absl::optional<float> RunQueryAndGetResult(
    UkmDatabase::CustomSqlQuery&& query) {
  absl::optional<float> output;
  UkmDatabase::QueryList queries;
  queries.emplace(0, std::move(query));
  base::RunLoop wait_for_query;
  UkmDatabase* database =
      UkmDatabaseClient::GetInstance().GetUkmDataManager()->GetUkmDatabase();
  database->RunReadonlyQueries(
      std::move(queries),
      base::BindOnce(
          [](base::OnceClosure quit, absl::optional<float>* output,
             bool success, processing::IndexedTensors tensor) {
            if (success) {
              EXPECT_EQ(1u, tensor.size());
              EXPECT_EQ(1u, tensor.at(0).size());
              *output = tensor.at(0)[0].float_val;
            }
            std::move(quit).Run();
          },
          wait_for_query.QuitClosure(), &output));
  wait_for_query.Run();
  return output;
}

}  // namespace

UkmDataManagerTestUtils::UkmDataManagerTestUtils(
    ukm::TestUkmRecorder* ukm_recorder)
    : ukm_recorder_(ukm_recorder) {}
UkmDataManagerTestUtils::~UkmDataManagerTestUtils() {
  UkmDatabaseClient::GetInstance().clear_ukm_recorder_for_testing();
}

void UkmDataManagerTestUtils::PreProfileInit(
    const std::map<SegmentId, proto::SegmentationModelMetadata>&
        default_overrides) {
  // Set test recorder before UkmObserver is created.
  UkmDatabaseClient::GetInstance().set_ukm_recorder_for_testing(ukm_recorder_);

  for (const auto& segment : default_overrides) {
    auto provider = std::make_unique<MockDefaultModelProvider>(segment.first,
                                                               segment.second);

    default_overrides_[segment.first] = provider.get();
    // Default model must be overridden before the platform is created:
    TestDefaultModelOverride::GetInstance().SetModelForTesting(
        segment.first, std::move(provider));
  }
}

void UkmDataManagerTestUtils::WaitForUkmObserverRegistration() {
  UkmObserver* observer =
      UkmDatabaseClient::GetInstance().ukm_observer_for_testing();
  while (!observer->is_started_for_testing()) {
    base::RunLoop().RunUntilIdle();
  }
}

proto::SegmentationModelMetadata
UkmDataManagerTestUtils::GetSamplePageLoadMetadata(const std::string& query) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.AddOutputConfigForBinaryClassifier(
      /*threshold=*/0.5f,
      /*positive_label=*/"Show",
      /*negative_label=*/"NotShow");
  metadata.set_time_unit(proto::TimeUnit::DAY);
  metadata.set_bucket_duration(42u);

  auto* feature = metadata.add_input_features();
  auto* sql_feature = feature->mutable_sql_feature();
  sql_feature->set_sql(query);

  auto* ukm_event = sql_feature->mutable_signal_filter()->add_ukm_events();
  ukm_event->set_event_hash(PageLoad::kEntryNameHash);
  ukm_event->add_metric_hash_filter(PageLoad::kCpuTimeNameHash);
  ukm_event->add_metric_hash_filter(PageLoad::kIsNewBookmarkNameHash);
  return metadata;
}

void UkmDataManagerTestUtils::RecordPageLoadUkm(const GURL& url,
                                                base::Time history_timestamp) {
  UkmObserver* observer =
      UkmDatabaseClient::GetInstance().ukm_observer_for_testing();
  // Ensure that the observer is started before recording metrics.
  ASSERT_TRUE(observer->is_started_for_testing());
  // Ensure that OTR profiles are not started in the test.
  ASSERT_FALSE(observer->is_paused_for_testing());

  ukm_recorder_->AddEntry(GetSamplePageLoadEntry(source_id_counter_));
  ukm_recorder_->UpdateSourceURL(source_id_counter_, url);
  source_id_counter_++;

  // Without a history service the recorded URLs will not be written to
  // database.
  ASSERT_TRUE(history_service_);
  history_service_->AddPage(url, history_timestamp,
                            history::VisitSource::SOURCE_BROWSED);
}

bool UkmDataManagerTestUtils::IsUrlInDatabase(const GURL& url) {
  UkmDatabase::CustomSqlQuery query("SELECT 1 FROM urls WHERE url=?",
                                    {processing::ProcessedValue(url.spec())});
  absl::optional<float> result = RunQueryAndGetResult(std::move(query));
  return !!result;
}

MockDefaultModelProvider* UkmDataManagerTestUtils::GetDefaultOverride(
    proto::SegmentId segment_id) {
  return default_overrides_[segment_id];
}

}  // namespace segmentation_platform
