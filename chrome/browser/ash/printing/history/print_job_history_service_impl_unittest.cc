// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"

#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/ash/printing/history/test_print_job_database.h"
#include "chrome/browser/ash/printing/history/test_print_job_history_service_observer.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTitle[] = "title";

const int kPagesNumber = 3;

}  // namespace

class PrintJobHistoryServiceImplTest : public ::testing::Test {
 public:
  PrintJobHistoryServiceImplTest() = default;

  void SetUp() override {
    test_prefs_.SetInitializationCompleted();
    PrintJobHistoryService::RegisterProfilePrefs(test_prefs_.registry());

    auto print_job_database = std::make_unique<TestPrintJobDatabase>();
    print_job_manager_ = std::make_unique<TestCupsPrintJobManager>(&profile_);
    print_job_history_service_ = std::make_unique<PrintJobHistoryServiceImpl>(
        std::move(print_job_database), print_job_manager_.get(), &test_prefs_);
  }

  void TearDown() override {
    print_job_history_service_.reset();
    print_job_manager_.reset();
  }

  void OnPrintJobSaved(base::RepeatingClosure run_loop_closure, bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  void OnPrintJobsRetrieved(
      base::RepeatingClosure run_loop_closure,
      bool success,
      std::vector<printing::proto::PrintJobInfo> entries) {
    EXPECT_TRUE(success);
    entries_ = std::move(entries);
    run_loop_closure.Run();
  }

  void OnDeleteAllPrintJobs(base::RepeatingClosure run_loop_closure,
                            bool success) {
    EXPECT_TRUE(success);
    run_loop_closure.Run();
  }

  std::vector<printing::proto::PrintJobInfo> GetPrintJobs() {
    base::RunLoop run_loop;
    print_job_history_service_->GetPrintJobs(
        base::BindOnce(&PrintJobHistoryServiceImplTest::OnPrintJobsRetrieved,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return entries_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestCupsPrintJobManager> print_job_manager_;
  std::unique_ptr<PrintJobHistoryService> print_job_history_service_;
  std::vector<printing::proto::PrintJobInfo> entries_;

 private:
  TestingProfile profile_;
  TestingPrefServiceSimple test_prefs_;
};

TEST_F(PrintJobHistoryServiceImplTest, SaveObservedCupsPrintJob) {
  base::RunLoop save_print_job_run_loop;
  TestPrintJobHistoryServiceObserver observer(
      print_job_history_service_.get(), save_print_job_run_loop.QuitClosure());

  std::unique_ptr<CupsPrintJob> print_job = std::make_unique<CupsPrintJob>(
      chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
      ::printing::PrintJob::Source::kPrintPreview,
      /*source_id=*/"", printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CancelPrintJob(print_job.get());
  save_print_job_run_loop.Run();

  std::vector<printing::proto::PrintJobInfo> entries = GetPrintJobs();

  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(kTitle, entries[0].title());
  EXPECT_EQ(kPagesNumber, entries[0].number_of_pages());
  EXPECT_EQ(printing::proto::PrintJobInfo_PrintJobStatus_CANCELED,
            entries[0].status());
}

TEST_F(PrintJobHistoryServiceImplTest, DoesNotSaveIncognitoPrintJobs) {
  // Expect no initial print jobs saved.
  std::vector<printing::proto::PrintJobInfo> entries = GetPrintJobs();
  EXPECT_EQ(0u, entries.size());

  auto print_job = std::make_unique<CupsPrintJob>(
      chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
      ::printing::PrintJob::Source::kPrintPreviewIncognito,
      /*source_id=*/"", printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CancelPrintJob(print_job.get());

  // Expect no print jobs saved from incognito source.
  entries = GetPrintJobs();
  EXPECT_EQ(0u, entries.size());
}

TEST_F(PrintJobHistoryServiceImplTest, ObserverTest) {
  base::RunLoop run_loop;
  TestPrintJobHistoryServiceObserver observer(print_job_history_service_.get(),
                                              run_loop.QuitClosure());

  std::unique_ptr<CupsPrintJob> print_job = std::make_unique<CupsPrintJob>(
      chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
      ::printing::PrintJob::Source::kPrintPreview,
      /*source_id=*/"", printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CancelPrintJob(print_job.get());
  run_loop.Run();

  EXPECT_EQ(1, observer.num_print_jobs());
}

TEST_F(PrintJobHistoryServiceImplTest, DeleteAllPrintJobs) {
  base::RunLoop save_print_job_run_loop;
  TestPrintJobHistoryServiceObserver observer(
      print_job_history_service_.get(), save_print_job_run_loop.QuitClosure());

  auto print_job = std::make_unique<CupsPrintJob>(
      chromeos::Printer(), /*job_id=*/0, kTitle, kPagesNumber,
      ::printing::PrintJob::Source::kPrintPreview,
      /*source_id=*/"", printing::proto::PrintSettings());
  print_job_manager_->CreatePrintJob(print_job.get());
  print_job_manager_->CancelPrintJob(print_job.get());
  save_print_job_run_loop.Run();

  std::vector<printing::proto::PrintJobInfo> entries = GetPrintJobs();
  EXPECT_EQ(1u, entries.size());

  // Delete all print jobs.
  base::RunLoop delete_all_print_jobs_run_loop;
  print_job_history_service_->DeleteAllPrintJobs(base::BindOnce(
      &PrintJobHistoryServiceImplTest::OnDeleteAllPrintJobs,
      base::Unretained(this), delete_all_print_jobs_run_loop.QuitClosure()));
  delete_all_print_jobs_run_loop.Run();

  // Run GetPrintJobs again and verify that the print job has been deleted.
  entries = GetPrintJobs();
  EXPECT_EQ(0u, entries.size());
}

}  // namespace ash
