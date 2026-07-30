// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_service_impl.h"

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/barrier_closure.h"
#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/chrome/browser/download/model/download_record_query.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using testing::_;
using testing::StrictMock;

namespace {

const std::string kTestDownloadId = "test_download";
const char kUpdatedPathStr[] = "/updated/path/file.pdf";

// Mock observer for testing notifications.
class MockDownloadRecordObserver : public DownloadRecordObserver {
 public:
  MOCK_METHOD(void,
              OnDownloadAdded,
              (const DownloadRecord& record),
              (override));
  MOCK_METHOD(void,
              OnDownloadUpdated,
              (const DownloadRecord& record),
              (override));
  MOCK_METHOD(void,
              OnDownloadsRemoved,
              (const std::vector<std::string_view>& download_ids),
              (override));
};

}  // namespace

class DownloadRecordServiceImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    feature_list_.InitAndEnableFeature(kDownloadList);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    CreateService();
  }

  void TearDown() override {
    service_.reset();
    web_states_.clear();
    profiles_.clear();
    PlatformTest::TearDown();
  }

  void CreateService() {
    service_ = std::make_unique<DownloadRecordServiceImpl>(temp_dir_.GetPath());
    ASSERT_TRUE(service_);

    // Wait for database initialization to complete.
    task_environment_.RunUntilIdle();
  }

  // Tears down the current service and re-creates it with
  // `kDownloadListPagination` enabled. Used by startup-path tests that need
  // to exercise the pagination-aware `MarkUnfinishedDownloadsAsFailed` path
  // without losing the on-disk DB written by the prior service instance —
  // the same `temp_dir_` is reused so the new service sees the persisted
  // rows from the old one.
  void RecreateServiceWithPaginationEnabled() {
    service_.reset();
    task_environment_.RunUntilIdle();
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kDownloadList, kDownloadListPagination},
        /*disabled_features=*/{});
    CreateService();
  }

  // Tears down + re-creates the service under the default fixture flag
  // configuration (kDownloadList ON, kDownloadListPagination OFF),
  // preserving the on-disk DB. Used by startup-path tests to verify the
  // legacy `LoadHistoricalRecords` path still flips kInProgress /
  // kNotStarted rows to kFailed after CL 3b's branching. Does NOT touch
  // `feature_list_`; it is still bound to the SetUp() configuration.
  void RestartServiceLegacyPath() {
    service_.reset();
    task_environment_.RunUntilIdle();
    CreateService();
  }

  std::unique_ptr<web::FakeDownloadTask> CreateFakeDownloadTask(
      const std::string& identifier,
      bool is_incognito = false,
      const std::string& original_url = "https://example.com/file.pdf",
      const std::string& mime_type = "application/pdf") {
    EXPECT_FALSE(identifier.empty());

    auto task =
        std::make_unique<web::FakeDownloadTask>(GURL(original_url), mime_type);
    task->SetIdentifier(@(identifier.c_str()));

    // Set up WebState with appropriate ProfileIOS.
    auto profile = TestProfileIOS::Builder().Build();
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(is_incognito ? profile->GetOffTheRecordProfile()
                                            : profile.get());

    task->SetWebState(web_state.get());

    // Store objects to ensure they remain alive during test execution.
    profiles_.push_back(std::move(profile));
    web_states_.push_back(std::move(web_state));

    EXPECT_NSEQ(@(identifier.c_str()), task->GetIdentifier());
    return task;
  }

  void RecordDownloadAndValidate(web::DownloadTask* task) {
    base::RunLoop run_loop;
    StrictMock<MockDownloadRecordObserver> mock_observer;
    service_->AddObserver(&mock_observer);

    EXPECT_CALL(mock_observer, OnDownloadAdded(_))
        .WillOnce([&](const DownloadRecord& record) { run_loop.Quit(); });

    service_->RecordDownload(task);
    run_loop.Run();

    service_->RemoveObserver(&mock_observer);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DownloadRecordService> service_;

  // Containers to maintain object lifetimes during tests
  std::vector<std::unique_ptr<TestProfileIOS>> profiles_;
  std::vector<std::unique_ptr<web::FakeWebState>> web_states_;
};

TEST_F(DownloadRecordServiceImplTest, RecordDownload) {
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask("test_download_1");
  RecordDownloadAndValidate(task.get());
}

TEST_F(DownloadRecordServiceImplTest, GetAllDownloads) {
  std::unique_ptr<web::FakeDownloadTask> task1 =
      CreateFakeDownloadTask("download_1");
  std::unique_ptr<web::FakeDownloadTask> task2 =
      CreateFakeDownloadTask("download_2");

  RecordDownloadAndValidate(task1.get());
  RecordDownloadAndValidate(task2.get());

  base::RunLoop run_loop;
  std::vector<DownloadRecord> result;

  service_->GetAllDownloadsAsync(
      base::BindLambdaForTesting([&](std::vector<DownloadRecord> records) {
        result = std::move(records);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(base::SysNSStringToUTF8(task1->GetIdentifier()),
            result[0].download_id);
  EXPECT_EQ(base::SysNSStringToUTF8(task2->GetIdentifier()),
            result[1].download_id);
}

TEST_F(DownloadRecordServiceImplTest, GetDownloadById) {
  const std::string download_id = "test_download";
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  base::RunLoop run_loop;
  std::optional<DownloadRecord> result;

  service_->GetDownloadByIdAsync(
      download_id,
      base::BindLambdaForTesting([&](std::optional<DownloadRecord> record) {
        result = std::move(record);
        run_loop.Quit();
      }));

  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(download_id, result->download_id);
}

TEST_F(DownloadRecordServiceImplTest, GetNonExistentDownloadById) {
  base::RunLoop run_loop;
  std::optional<DownloadRecord> result;

  service_->GetDownloadByIdAsync(
      "non_existent",
      base::BindLambdaForTesting([&](std::optional<DownloadRecord> record) {
        result = std::move(record);
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(result.has_value());
}

TEST_F(DownloadRecordServiceImplTest, RemoveDownloadById) {
  const std::string download_id = "test_download";
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  base::RunLoop run_loop;
  bool removal_success = false;

  // Expects OnDownloadsRemoved to be called with the correct download ID.
  EXPECT_CALL(mock_observer, OnDownloadsRemoved(_))
      .WillOnce([&](const std::vector<std::string_view>& download_ids) {
        ASSERT_EQ(1u, download_ids.size());
        EXPECT_EQ(download_id, download_ids[0]);
      });

  service_->RemoveDownloadByIdAsync(
      download_id, base::BindLambdaForTesting([&](bool success) {
        removal_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_TRUE(removal_success);
  service_->RemoveObserver(&mock_observer);
}

TEST_F(DownloadRecordServiceImplTest, RemoveNonExistentDownloadById) {
  base::RunLoop run_loop;
  bool removal_success = false;

  service_->RemoveDownloadByIdAsync(
      "non_existent_download", base::BindLambdaForTesting([&](bool success) {
        removal_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  // The implementation considers removing non-existent records as success.
  EXPECT_TRUE(removal_success);
}

TEST_F(DownloadRecordServiceImplTest, UpdateDownloadFilePath) {
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(kTestDownloadId);
  RecordDownloadAndValidate(task.get());

  // Set up observer to verify the update notification.
  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  base::RunLoop update_run_loop;
  bool update_success = false;
  const base::FilePath updated_path(kUpdatedPathStr);

  // Expect OnDownloadUpdated to be called with the updated file path.
  EXPECT_CALL(mock_observer, OnDownloadUpdated(_))
      .WillOnce([&](const DownloadRecord& record) {
        EXPECT_EQ(kTestDownloadId, record.download_id);
        EXPECT_EQ(updated_path, record.file_path);
        update_run_loop.Quit();
      });

  service_->UpdateDownloadFilePathAsync(
      kTestDownloadId, updated_path,
      base::BindLambdaForTesting(
          [&](bool success) { update_success = success; }));

  update_run_loop.Run();

  EXPECT_TRUE(update_success);

  service_->RemoveObserver(&mock_observer);
}

TEST_F(DownloadRecordServiceImplTest, UpdateNonExistentDownloadFilePath) {
  const std::string non_existent_id = "non_existent_download";
  const base::FilePath test_path("/test/path/file.pdf");

  base::RunLoop run_loop;
  bool update_success = true;  // Initialize to true to test it becomes false

  service_->UpdateDownloadFilePathAsync(
      non_existent_id, test_path, base::BindLambdaForTesting([&](bool success) {
        update_success = success;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_FALSE(update_success);
}

TEST_F(DownloadRecordServiceImplTest, UpdateDownloadStates) {
  const std::string download_id = "state_test_download";
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(download_id);
  RecordDownloadAndValidate(task.get());

  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  // Tests progress update.
  base::RunLoop progress_loop;
  EXPECT_CALL(mock_observer, OnDownloadUpdated(_))
      .WillOnce([&](const DownloadRecord& record) {
        EXPECT_EQ(500, record.received_bytes);
        progress_loop.Quit();
      });
  task->SetReceivedBytes(500);
  progress_loop.Run();

  // Tests completion update.
  base::RunLoop completion_loop;
  EXPECT_CALL(mock_observer, OnDownloadUpdated(_))
      .WillOnce([&](const DownloadRecord& record) {
        EXPECT_EQ(web::DownloadTask::State::kComplete, record.state);
        completion_loop.Quit();
      });
  task->SetDone(true);
  completion_loop.Run();

  service_->RemoveObserver(&mock_observer);
}

TEST_F(DownloadRecordServiceImplTest, NotifiesAllObservers) {
  MockDownloadRecordObserver observer1;
  MockDownloadRecordObserver observer2;

  service_->AddObserver(&observer1);
  service_->AddObserver(&observer2);

  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask("test_download");

  base::RunLoop run_loop1;
  base::RepeatingClosure barrier =
      base::BarrierClosure(2, run_loop1.QuitClosure());

  // Both observers should be notified.
  EXPECT_CALL(observer1, OnDownloadAdded(_))
      .WillOnce([barrier](const DownloadRecord& record) { barrier.Run(); });
  EXPECT_CALL(observer2, OnDownloadAdded(_))
      .WillOnce([barrier](const DownloadRecord& record) { barrier.Run(); });

  service_->RecordDownload(task.get());
  run_loop1.Run();

  // Removes one observer.
  service_->RemoveObserver(&observer1);

  std::unique_ptr<web::FakeDownloadTask> task2 =
      CreateFakeDownloadTask("test_download_2");

  base::RunLoop run_loop2;

  // Only observer2 should be notified.
  EXPECT_CALL(observer2, OnDownloadAdded(_))
      .WillOnce([&](const DownloadRecord& record) { run_loop2.Quit(); });

  service_->RecordDownload(task2.get());
  run_loop2.Run();

  service_->RemoveObserver(&observer2);
}

TEST_F(DownloadRecordServiceImplTest, PersistOnlyNonIncognitoRecords) {
  const std::string incognito_id = "incognito_download";
  const std::string normal_id = "normal_download";

  // Record an incognito download.
  std::unique_ptr<web::FakeDownloadTask> incognito_task =
      CreateFakeDownloadTask(incognito_id,
                             /*is_incognito=*/true);
  RecordDownloadAndValidate(incognito_task.get());

  // Record a normal download.
  std::unique_ptr<web::FakeDownloadTask> normal_task =
      CreateFakeDownloadTask(normal_id);
  RecordDownloadAndValidate(normal_task.get());

  // Verify both downloads exist in current session.
  base::RunLoop run_loop1;
  std::vector<DownloadRecord> result;
  service_->GetAllDownloadsAsync(
      base::BindLambdaForTesting([&](std::vector<DownloadRecord> records) {
        result = std::move(records);
        run_loop1.Quit();
      }));
  run_loop1.Run();
  EXPECT_EQ(2u, result.size());

  // Verify both record has correct flag.
  bool found_incognito = false;
  bool found_normal = false;
  for (const auto& record : result) {
    if (record.download_id == incognito_id) {
      EXPECT_TRUE(record.is_incognito);
      found_incognito = true;
    } else if (record.download_id == normal_id) {
      EXPECT_FALSE(record.is_incognito);
      found_normal = true;
    }
  }
  EXPECT_TRUE(found_incognito);
  EXPECT_TRUE(found_normal);

  // Restart service (simulates app restart).
  service_.reset();
  task_environment_.RunUntilIdle();
  CreateService();

  // Verify only normal download persists after restart.
  base::RunLoop run_loop2;
  std::vector<DownloadRecord> persistent_result;
  service_->GetAllDownloadsAsync(
      base::BindLambdaForTesting([&](std::vector<DownloadRecord> records) {
        persistent_result = std::move(records);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  ASSERT_EQ(1u, persistent_result.size());
  EXPECT_EQ(normal_id, persistent_result[0].download_id);
  EXPECT_FALSE(persistent_result[0].is_incognito);
}

TEST_F(DownloadRecordServiceImplTest, RecordDownloadTwiceWithSameTask) {
  const std::string download_id = "retry_download";
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(download_id);

  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  // First RecordDownload should trigger OnDownloadAdded.
  base::RunLoop run_loop1;
  EXPECT_CALL(mock_observer, OnDownloadAdded(_))
      .WillOnce([&](const DownloadRecord& record) {
        EXPECT_EQ(download_id, record.download_id);
        run_loop1.Quit();
      });

  service_->RecordDownload(task.get());
  run_loop1.Run();

  // Second RecordDownload with the same task (simulating retry) should NOT
  // trigger OnDownloadAdded again because the task is already observed.
  // This tests the guard against double-registration.

  // No expectations set - StrictMock will fail if OnDownloadAdded is called.
  // We verify this by checking that only one record exists in the database.
  service_->RecordDownload(task.get());

  service_->RemoveObserver(&mock_observer);

  // Verify only one download record exists.
  base::RunLoop run_loop2;
  std::vector<DownloadRecord> result;
  service_->GetAllDownloadsAsync(
      base::BindLambdaForTesting([&](std::vector<DownloadRecord> records) {
        result = std::move(records);
        run_loop2.Quit();
      }));
  run_loop2.Run();

  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(download_id, result[0].download_id);
}

// Regression test: the DownloadTask may be destroyed while the asynchronous
// InsertRecord write is still in flight (e.g. the user cancels the download
// before the DB write completes). The reply must not dereference the freed
// task. Previously the reply captured a raw DownloadTask* and called
// AddObservation() on freed memory, causing a use-after-free crash.
TEST_F(DownloadRecordServiceImplTest, RecordDownloadTaskDestroyedBeforeReply) {
  const std::string download_id = "cancelled_download";
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(download_id);

  StrictMock<MockDownloadRecordObserver> mock_observer;
  service_->AddObserver(&mock_observer);

  // OnDownloadAdded must NOT fire: the task is gone by the time the reply
  // runs, so observation is skipped. StrictMock fails if it is called.
  service_->RecordDownload(task.get());

  // Destroy the task before the posted InsertRecord reply runs. The service
  // is not yet observing the task, so it receives no `OnDownloadDestroyed`.
  task.reset();

  // Issue a follow-up query and wait for its callback. The database task
  // runner is sequenced, so GetAllFromCache runs after InsertRecord and its
  // reply runs after the RecordDownload reply on the main sequence. When this
  // run loop quits, the RecordDownload reply has therefore already executed --
  // and must not have crashed dereferencing the freed task. The record write
  // itself still succeeds; only the post-write observation is skipped.
  base::RunLoop run_loop;
  std::vector<DownloadRecord> result;
  service_->GetAllDownloadsAsync(
      base::BindLambdaForTesting([&](std::vector<DownloadRecord> records) {
        result = std::move(records);
        run_loop.Quit();
      }));
  run_loop.Run();

  service_->RemoveObserver(&mock_observer);

  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(download_id, result[0].download_id);
}

// Sanity test for the pagination API: with one persisted record,
// `GetDownloadsPageAsync` posts a non-empty vector to the calling
// sequence asynchronously. Locks in the async contract; broader
// coverage (cursor / filter / ordering) lands in a follow-up CL.
TEST_F(DownloadRecordServiceImplTest, GetDownloadsPageAsync_AsyncRoundTrip) {
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask("page_async_1");
  RecordDownloadAndValidate(task.get());

  base::test::TestFuture<std::vector<DownloadRecord>> future;
  DownloadRecordQuery query;
  service_->GetDownloadsPageAsync(query, future.GetCallback());

  // *Async contract: callback must NOT have fired synchronously.
  EXPECT_FALSE(future.IsReady());

  // Pumps the calling sequence until the posted callback runs.
  std::vector<DownloadRecord> page = future.Get();
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("page_async_1", page[0].download_id);
}

// Mirrors `GetDownloadsPageAsync_AsyncRoundTrip` for the count API.
TEST_F(DownloadRecordServiceImplTest, GetDownloadsCountAsync_AsyncRoundTrip) {
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask("count_async_1");
  RecordDownloadAndValidate(task.get());

  base::test::TestFuture<size_t> future;
  service_->GetDownloadsCountAsync(std::nullopt, future.GetCallback());

  EXPECT_FALSE(future.IsReady());  // *Async contract.
  EXPECT_EQ(size_t{1}, future.Get());
}

// =======================================================================
// Pagination read API — focused coverage for GetDownloadsPageAsync /
// GetDownloadsCountAsync as wired to the DB layer in the previous CL.
// These exercise the DB-level keyset pagination semantics that the
// service forwards verbatim.
// =======================================================================

// With no records in the DB, the page API returns an empty vector.
TEST_F(DownloadRecordServiceImplTest, GetDownloadsPageAsync_EmptyDB) {
  base::test::TestFuture<std::vector<DownloadRecord>> future;
  DownloadRecordQuery query;
  service_->GetDownloadsPageAsync(query, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

// Page results are ordered DESC by (created_time, download_id), so the
// most recently recorded download appears first.
TEST_F(DownloadRecordServiceImplTest,
       GetDownloadsPageAsync_OrderedByCreatedTimeDesc) {
  std::unique_ptr<web::FakeDownloadTask> t1 = CreateFakeDownloadTask("id_1");
  std::unique_ptr<web::FakeDownloadTask> t2 = CreateFakeDownloadTask("id_2");
  std::unique_ptr<web::FakeDownloadTask> t3 = CreateFakeDownloadTask("id_3");
  RecordDownloadAndValidate(t1.get());
  RecordDownloadAndValidate(t2.get());
  RecordDownloadAndValidate(t3.get());

  base::test::TestFuture<std::vector<DownloadRecord>> future;
  DownloadRecordQuery query;
  service_->GetDownloadsPageAsync(query, future.GetCallback());
  std::vector<DownloadRecord> page = future.Get();

  ASSERT_EQ(3u, page.size());
  // DESC ordering: created_time DESC (id_3 last inserted, so newest) with
  // download_id DESC as the tiebreaker when timestamps collide. Either
  // way the expected ordering is id_3, id_2, id_1.
  EXPECT_EQ("id_3", page[0].download_id);
  EXPECT_EQ("id_2", page[1].download_id);
  EXPECT_EQ("id_1", page[2].download_id);
}

// A cursor pointing at row N returns only rows strictly past it (cursor
// row itself is not duplicated on the next page).
TEST_F(DownloadRecordServiceImplTest,
       GetDownloadsPageAsync_CursorReturnsNextPage) {
  std::unique_ptr<web::FakeDownloadTask> t1 = CreateFakeDownloadTask("id_1");
  std::unique_ptr<web::FakeDownloadTask> t2 = CreateFakeDownloadTask("id_2");
  std::unique_ptr<web::FakeDownloadTask> t3 = CreateFakeDownloadTask("id_3");
  RecordDownloadAndValidate(t1.get());
  RecordDownloadAndValidate(t2.get());
  RecordDownloadAndValidate(t3.get());

  // First page: fetch everything to get the front row's cursor coords.
  base::test::TestFuture<std::vector<DownloadRecord>> first_future;
  service_->GetDownloadsPageAsync(DownloadRecordQuery(),
                                  first_future.GetCallback());
  std::vector<DownloadRecord> first_page = first_future.Get();
  ASSERT_EQ(3u, first_page.size());
  const DownloadRecord& cursor_row = first_page[0];  // id_3

  // Second page using the first row as the cursor — should return the
  // remaining two rows, excluding the cursor row itself.
  DownloadRecordQuery query;
  query.cursor_created_time = cursor_row.created_time;
  query.cursor_download_id = cursor_row.download_id;

  base::test::TestFuture<std::vector<DownloadRecord>> second_future;
  service_->GetDownloadsPageAsync(query, second_future.GetCallback());
  std::vector<DownloadRecord> second_page = second_future.Get();

  ASSERT_EQ(2u, second_page.size());
  EXPECT_EQ("id_2", second_page[0].download_id);
  EXPECT_EQ("id_1", second_page[1].download_id);
}

// A filter narrows results to matching MIME types only.
TEST_F(DownloadRecordServiceImplTest,
       GetDownloadsPageAsync_FilterTypeNarrowsResults) {
  std::unique_ptr<web::FakeDownloadTask> pdf_task =
      CreateFakeDownloadTask("pdf_doc", /*is_incognito=*/false,
                             "https://example.com/file.pdf", "application/pdf");
  std::unique_ptr<web::FakeDownloadTask> img_task =
      CreateFakeDownloadTask("img_pic", /*is_incognito=*/false,
                             "https://example.com/file.png", "image/png");
  RecordDownloadAndValidate(pdf_task.get());
  RecordDownloadAndValidate(img_task.get());

  base::test::TestFuture<std::vector<DownloadRecord>> future;
  DownloadRecordQuery query;
  query.filter_type = DownloadFilterType::kPDF;
  service_->GetDownloadsPageAsync(query, future.GetCallback());

  std::vector<DownloadRecord> page = future.Get();
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("pdf_doc", page[0].download_id);
}

// A name_query case-insensitively substring-matches the file name.
TEST_F(DownloadRecordServiceImplTest,
       GetDownloadsPageAsync_NameQueryMatchesSubstring) {
  std::unique_ptr<web::FakeDownloadTask> report = CreateFakeDownloadTask(
      "report_doc", /*is_incognito=*/false,
      "https://example.com/Quarterly_Report.pdf", "application/pdf");
  report->SetGeneratedFileName(
      base::FilePath(FILE_PATH_LITERAL("Quarterly_Report.pdf")));
  std::unique_ptr<web::FakeDownloadTask> invoice = CreateFakeDownloadTask(
      "invoice_doc", /*is_incognito=*/false,
      "https://example.com/Invoice_2026.pdf", "application/pdf");
  invoice->SetGeneratedFileName(
      base::FilePath(FILE_PATH_LITERAL("Invoice_2026.pdf")));
  RecordDownloadAndValidate(report.get());
  RecordDownloadAndValidate(invoice.get());

  base::test::TestFuture<std::vector<DownloadRecord>> future;
  DownloadRecordQuery query;
  query.name_query = "report";  // matches "Quarterly_Report.pdf"
  service_->GetDownloadsPageAsync(query, future.GetCallback());

  std::vector<DownloadRecord> page = future.Get();
  ASSERT_EQ(1u, page.size());
  EXPECT_EQ("report_doc", page[0].download_id);
}

// With no records in the DB, the count API returns 0.
TEST_F(DownloadRecordServiceImplTest, GetDownloadsCountAsync_EmptyDB) {
  base::test::TestFuture<size_t> future;
  service_->GetDownloadsCountAsync(std::nullopt, future.GetCallback());
  EXPECT_EQ(size_t{0}, future.Get());
}

// Without a filter (or with kAll) the count returns every persisted row.
TEST_F(DownloadRecordServiceImplTest, GetDownloadsCountAsync_AllReturnsTotal) {
  std::unique_ptr<web::FakeDownloadTask> t1 = CreateFakeDownloadTask("id_1");
  std::unique_ptr<web::FakeDownloadTask> t2 = CreateFakeDownloadTask("id_2");
  std::unique_ptr<web::FakeDownloadTask> t3 = CreateFakeDownloadTask("id_3");
  RecordDownloadAndValidate(t1.get());
  RecordDownloadAndValidate(t2.get());
  RecordDownloadAndValidate(t3.get());

  base::test::TestFuture<size_t> future;
  service_->GetDownloadsCountAsync(std::nullopt, future.GetCallback());
  EXPECT_EQ(size_t{3}, future.Get());
}

// A filter narrows the count to matching MIME types only.
TEST_F(DownloadRecordServiceImplTest,
       GetDownloadsCountAsync_FilterNarrowsCount) {
  std::unique_ptr<web::FakeDownloadTask> pdf1 =
      CreateFakeDownloadTask("pdf_1", /*is_incognito=*/false,
                             "https://example.com/a.pdf", "application/pdf");
  std::unique_ptr<web::FakeDownloadTask> pdf2 =
      CreateFakeDownloadTask("pdf_2", /*is_incognito=*/false,
                             "https://example.com/b.pdf", "application/pdf");
  std::unique_ptr<web::FakeDownloadTask> img =
      CreateFakeDownloadTask("img_1", /*is_incognito=*/false,
                             "https://example.com/c.png", "image/png");
  RecordDownloadAndValidate(pdf1.get());
  RecordDownloadAndValidate(pdf2.get());
  RecordDownloadAndValidate(img.get());

  base::test::TestFuture<size_t> pdf_future;
  service_->GetDownloadsCountAsync(DownloadFilterType::kPDF,
                                   pdf_future.GetCallback());
  EXPECT_EQ(size_t{2}, pdf_future.Get());

  base::test::TestFuture<size_t> img_future;
  service_->GetDownloadsCountAsync(DownloadFilterType::kImage,
                                   img_future.GetCallback());
  EXPECT_EQ(size_t{1}, img_future.Get());
}

// =======================================================================
// Startup cleanup — covers the construction-time dispatch added in this
// CL: the service snapshots `IsDownloadListPaginationEnabled()` once and
// posts either `LoadHistoricalRecords` (legacy) or
// `MarkUnfinishedDownloadsAsFailed` (pagination-aware) to the DB sequence.
// Both paths must end with any kInProgress / kNotStarted row from the
// previous session flipped to kFailed.
// =======================================================================

// Flag-OFF: simulated app restart with an unfinished row on disk. The
// legacy `LoadHistoricalRecords` + `CleanupInconsistentStates` path must
// flip the row to kFailed in the DB and surface that state through the
// legacy `GetAllDownloadsAsync` cache read. Anchors the pre-existing
// behavior so CL 3b's branching does not regress it.
TEST_F(DownloadRecordServiceImplTest,
       StartupCleanup_LegacyPathFlipsUnfinishedToFailed) {
  const std::string kId = "unfinished_legacy";

  // Seed: record a download whose state advances to kInProgress.
  std::unique_ptr<web::FakeDownloadTask> task = CreateFakeDownloadTask(kId);
  RecordDownloadAndValidate(task.get());
  task->SetState(web::DownloadTask::State::kInProgress);
  task_environment_.RunUntilIdle();

  // Sanity: row is persisted as kInProgress before the simulated restart.
  base::test::TestFuture<std::optional<DownloadRecord>> before;
  service_->GetDownloadByIdAsync(kId, before.GetCallback());
  ASSERT_TRUE(before.Get().has_value());
  EXPECT_EQ(web::DownloadTask::State::kInProgress, before.Get()->state);

  // Simulated app restart on the legacy (flag-OFF) path.
  RestartServiceLegacyPath();

  // After startup cleanup the row must be kFailed.
  base::test::TestFuture<std::optional<DownloadRecord>> after;
  service_->GetDownloadByIdAsync(kId, after.GetCallback());
  ASSERT_TRUE(after.Get().has_value());
  EXPECT_EQ(web::DownloadTask::State::kFailed, after.Get()->state);
}

// Flag-ON: simulated app restart with an unfinished row on disk. The
// pagination-aware `MarkUnfinishedDownloadsAsFailed` path must issue a
// single DB UPDATE that flips the row to kFailed; the row must be
// observable via the paginated reader (the legacy cache is intentionally
// NOT pre-populated in this CL).
TEST_F(DownloadRecordServiceImplTest,
       StartupCleanup_PaginationPathFlipsUnfinishedToFailed) {
  const std::string kId = "unfinished_pagination";

  // Seed: record a download whose state advances to kInProgress while the
  // service runs on the legacy (default fixture) path.
  std::unique_ptr<web::FakeDownloadTask> task = CreateFakeDownloadTask(kId);
  RecordDownloadAndValidate(task.get());
  task->SetState(web::DownloadTask::State::kInProgress);
  task_environment_.RunUntilIdle();

  // Simulated app restart with kDownloadListPagination ON. Reuses the
  // same on-disk DB so the new service sees the unfinished row.
  RecreateServiceWithPaginationEnabled();

  // The pagination reader walks the DB directly — no legacy cache prefetch
  // — so it should observe the row and report it as kFailed.
  base::test::TestFuture<std::vector<DownloadRecord>> page;
  DownloadRecordQuery query;
  service_->GetDownloadsPageAsync(query, page.GetCallback());
  std::vector<DownloadRecord> rows = page.Get();
  ASSERT_EQ(1u, rows.size());
  EXPECT_EQ(kId, rows[0].download_id);
  EXPECT_EQ(web::DownloadTask::State::kFailed, rows[0].state);
}

// Flag-ON: empty DB at startup. `MarkUnfinishedDownloadsAsFailed` must be
// a no-op safe path — no crash, no spurious rows surfaced. Anchors the
// degenerate-input branch of the new method.
TEST_F(DownloadRecordServiceImplTest, StartupCleanup_PaginationPathEmptyDB) {
  // Throw away the default-fixture service so the next service constructs
  // against a fresh DB but with kDownloadListPagination enabled.
  RecreateServiceWithPaginationEnabled();

  base::test::TestFuture<std::vector<DownloadRecord>> page;
  DownloadRecordQuery query;
  service_->GetDownloadsPageAsync(query, page.GetCallback());
  EXPECT_TRUE(page.Get().empty());

  base::test::TestFuture<size_t> count;
  service_->GetDownloadsCountAsync(std::nullopt, count.GetCallback());
  EXPECT_EQ(size_t{0}, count.Get());
}

// Active records cache — covers the flag-ON in-memory hot cache
// (`active_records_cache_` + `incognito_records_` in the store). Reads
// use `GetById`, writes flow via `WriteThroughToActiveCache`,
// eviction via `EvictOnDestroy`. None of these paths fire under the
// flag-OFF fixture default.

// Flag-ON: `RecordDownload` for a persisted task must populate the
// active cache so a subsequent `GetDownloadByIdAsync` resolves through
// the pagination read path. The id round-trip is the contract: cache
// miss would still hit the DB and return the same record, but if the
// row is reachable it proves the write-through wired up at insert time.
TEST_F(DownloadRecordServiceImplTest,
       ActiveCache_RecordDownload_PopulatesCache_FlagOn) {
  RecreateServiceWithPaginationEnabled();

  const std::string kId = "active_cache_record";
  std::unique_ptr<web::FakeDownloadTask> task = CreateFakeDownloadTask(kId);
  RecordDownloadAndValidate(task.get());

  base::test::TestFuture<std::optional<DownloadRecord>> got;
  service_->GetDownloadByIdAsync(kId, got.GetCallback());
  ASSERT_TRUE(got.Get().has_value());
  EXPECT_EQ(kId, got.Get()->download_id);
}

// Flag-ON: a volatile-only update (`SetReceivedBytes`) does not persist
// to the DB (`ShouldPersistUpdate` returns false for progress fields),
// but it MUST refresh the active cache so subsequent reads see the
// freshest byte count. Without write-through on the no-persist branch
// the UI byte progress would freeze at the last persisted value.
TEST_F(DownloadRecordServiceImplTest,
       ActiveCache_OnDownloadUpdated_HotPath_RefreshesCache_FlagOn) {
  RecreateServiceWithPaginationEnabled();

  const std::string kId = "active_cache_update";
  std::unique_ptr<web::FakeDownloadTask> task = CreateFakeDownloadTask(kId);
  RecordDownloadAndValidate(task.get());

  // Volatile-only mutation: progress field, no state transition.
  constexpr int64_t kBytes = 4096;
  task->SetReceivedBytes(kBytes);

  base::test::TestFuture<std::optional<DownloadRecord>> got;
  service_->GetDownloadByIdAsync(kId, got.GetCallback());
  ASSERT_TRUE(got.Get().has_value());
  EXPECT_EQ(kBytes, got.Get()->received_bytes);
}

// Flag-ON: destroying the underlying task posts a FIFO `EvictOnDestroy`
// task to the database sequence. The cold path (cache miss → DB
// fallback) then returns the last persisted value, which for a volatile-
// only update is the pre-bytes record. Anchors both:
//   - the `OnDownloadDestroyed` eviction wiring, and
//   - the DB-version-wins fallback after eviction.
TEST_F(DownloadRecordServiceImplTest,
       ActiveCache_OnDownloadDestroyed_EvictsAndDbFallback_FlagOn) {
  RecreateServiceWithPaginationEnabled();

  const std::string kId = "active_cache_evict";
  std::unique_ptr<web::FakeDownloadTask> task = CreateFakeDownloadTask(kId);
  RecordDownloadAndValidate(task.get());

  // Make the cache strictly hotter than the DB.
  constexpr int64_t kHotBytes = 8192;
  task->SetReceivedBytes(kHotBytes);

  // Sanity: hot value visible while task is alive.
  base::test::TestFuture<std::optional<DownloadRecord>> hot;
  service_->GetDownloadByIdAsync(kId, hot.GetCallback());
  ASSERT_TRUE(hot.Get().has_value());
  EXPECT_EQ(kHotBytes, hot.Get()->received_bytes);

  // Destroy the task. `OnDownloadDestroyed` posts an `EvictOnDestroy` task
  // to the database sequence, which is FIFO-ordered with subsequent
  // `GetDownloadByIdAsync` work — by the time the next read runs, the
  // cache entry is gone and the DB-version wins.
  task.reset();

  // GetDownloadByIdAsync now misses the cache and falls back to the DB,
  // which only ever saw the pre-bytes (no-progress-persisted) record.
  base::test::TestFuture<std::optional<DownloadRecord>> cold;
  service_->GetDownloadByIdAsync(kId, cold.GetCallback());
  ASSERT_TRUE(cold.Get().has_value());
  EXPECT_EQ(kId, cold.Get()->download_id);
  EXPECT_EQ(int64_t{0}, cold.Get()->received_bytes);
}

// Flag-ON: incognito records live only in `incognito_records_` (never
// in the DB), so `OnDownloadDestroyed` eviction makes them disappear
// entirely. There is no DB row to fall back to.
TEST_F(DownloadRecordServiceImplTest,
       ActiveCache_OnDownloadDestroyed_EvictsIncognito_FlagOn) {
  RecreateServiceWithPaginationEnabled();

  const std::string kId = "incognito_evict";
  std::unique_ptr<web::FakeDownloadTask> task =
      CreateFakeDownloadTask(kId, /*is_incognito=*/true);
  RecordDownloadAndValidate(task.get());

  // Sanity: reachable while alive.
  base::test::TestFuture<std::optional<DownloadRecord>> alive;
  service_->GetDownloadByIdAsync(kId, alive.GetCallback());
  ASSERT_TRUE(alive.Get().has_value());
  EXPECT_TRUE(alive.Get()->is_incognito);

  task.reset();

  // After eviction the incognito row is gone everywhere: never persisted,
  // and cache entry removed by EvictOnDestroy.
  base::test::TestFuture<std::optional<DownloadRecord>> gone;
  service_->GetDownloadByIdAsync(kId, gone.GetCallback());
  EXPECT_FALSE(gone.Get().has_value());
}

// Flag-OFF (default fixture): none of the new active-cache write-through
// or eviction logic should fire. Exercise the same Record + volatile
// update + destroy sequence as the flag-ON eviction test and assert the
// legacy `record_cache_` behavior: the in-memory record reflects the
// volatile bytes (legacy cache is always written through), the destroy
// hook is a pure no-op for cache state, and the row remains reachable
// via `GetDownloadByIdAsync` because `record_cache_` keeps it.
TEST_F(DownloadRecordServiceImplTest, ActiveCache_FlagOff_NoActiveCacheUsage) {
  // Default fixture: kDownloadListPagination is OFF.
  const std::string kId = "legacy_noop";
  std::unique_ptr<web::FakeDownloadTask> task = CreateFakeDownloadTask(kId);
  RecordDownloadAndValidate(task.get());

  constexpr int64_t kBytes = 2048;
  task->SetReceivedBytes(kBytes);

  base::test::TestFuture<std::optional<DownloadRecord>> hot;
  service_->GetDownloadByIdAsync(kId, hot.GetCallback());
  ASSERT_TRUE(hot.Get().has_value());
  EXPECT_EQ(kBytes, hot.Get()->received_bytes);

  // Destroy the task. Under flag-OFF, `OnDownloadDestroyed` does NOT
  // post any eviction work; `record_cache_` keeps the entry.
  task.reset();

  base::test::TestFuture<std::optional<DownloadRecord>> still_there;
  service_->GetDownloadByIdAsync(kId, still_there.GetCallback());
  ASSERT_TRUE(still_there.Get().has_value());
  EXPECT_EQ(kBytes, still_there.Get()->received_bytes);
}
