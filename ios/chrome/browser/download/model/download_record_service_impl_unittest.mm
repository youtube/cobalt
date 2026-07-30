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
