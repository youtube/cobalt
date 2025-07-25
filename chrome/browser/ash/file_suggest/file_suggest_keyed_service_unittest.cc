// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service_observer.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::test {
// TODO(https://crbug.com/1370774): move `ScopedTestMountPoint` out of holding
// space to remove the dependency on holding space code.
using ash::holding_space::ScopedTestMountPoint;

class FileSuggestKeyedServiceTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "primary_profile@test", GetTestingFactories());
    WaitUntilFileSuggestServiceReady(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_));
  }

  virtual TestingProfile::TestingFactories GetTestingFactories() { return {}; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile, ExperimentalAsh> profile_ = nullptr;
};

TEST_F(FileSuggestKeyedServiceTest, GetSuggestData) {
  base::HistogramTester tester;
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile_)
      ->GetSuggestFileData(
          FileSuggestionType::kDriveFile,
          base::BindOnce([](const absl::optional<std::vector<FileSuggestData>>&
                                suggest_data) {
            EXPECT_FALSE(suggest_data.has_value());
          }));
  tester.ExpectBucketCount(
      "Ash.Search.DriveFileSuggestDataValidation.Status",
      /*sample=*/DriveSuggestValidationStatus::kDriveFSNotMounted,
      /*expected_count=*/1);
}

class FileSuggestKeyedServiceRemoveTest : public FileSuggestKeyedServiceTest {
 protected:
  // FileSuggestKeyedServiceTest:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FileSuggestKeyedServiceTest::SetUp();
    file_suggest_service_ = static_cast<MockFileSuggestKeyedService*>(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_));

    // Initialize the local file mount point.
    local_fs_mount_point_ = std::make_unique<ScopedTestMountPoint>(
        /*name=*/"test_mount", storage::kFileSystemTypeLocal,
        file_manager::VOLUME_TYPE_TESTING);
    local_fs_mount_point_->Mount(profile_);

    // Initialize the drive file mount point.
    drive_fs_mount_point_ = std::make_unique<ScopedTestMountPoint>(
        /*name=*/"drivefs-delayed_mount", storage::kFileSystemTypeDriveFs,
        file_manager::VOLUME_TYPE_GOOGLE_DRIVE);
    drive_fs_mount_point_->Mount(profile_);
  }

  void TearDown() override {
    drive_fs_mount_point_.reset();
    local_fs_mount_point_.reset();
    FileSuggestKeyedServiceTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{FileSuggestKeyedServiceFactory::GetInstance(),
             base::BindRepeating(
                 &MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService,
                 temp_dir_.GetPath().Append("proto"))}};
  }

  absl::optional<std::vector<FileSuggestData>> GetSuggestionsForType(
      FileSuggestionType type) {
    absl::optional<std::vector<FileSuggestData>> suggestions;
    base::RunLoop run_loop;
    file_suggest_service_->GetSuggestFileData(
        type, base::BindOnce(
                  [](base::RunLoop* run_loop,
                     absl::optional<std::vector<FileSuggestData>>* suggestions,
                     const absl::optional<std::vector<FileSuggestData>>&
                         fetched_suggestions) {
                    *suggestions = fetched_suggestions;
                    run_loop->Quit();
                  },
                  &run_loop, &suggestions));
    run_loop.Run();
    return suggestions;
  }

  // Creates files and suggests these files through the file suggest keyed
  // service. Returns paths to these files.
  std::vector<base::FilePath> PrepareFileSuggestionsForType(
      FileSuggestionType type,
      size_t count) {
    ScopedTestMountPoint* mount_point = nullptr;
    switch (type) {
      case FileSuggestionType::kDriveFile:
        mount_point = drive_fs_mount_point_.get();
        break;
      case FileSuggestionType::kLocalFile:
        mount_point = local_fs_mount_point_.get();
        break;
    }

    std::vector<base::FilePath> suggested_file_paths;
    std::vector<FileSuggestData> suggestions;
    for (size_t index = 0; index < count; ++index) {
      suggested_file_paths.push_back(mount_point->CreateArbitraryFile());
      suggestions.emplace_back(type, suggested_file_paths.back(),
                               /*new_prediction_reason=*/absl::nullopt,
                               /*new_score=*/absl::nullopt);
    }
    file_suggest_service_->SetSuggestionsForType(type, suggestions);
    return suggested_file_paths;
  }

  // Hosts the proto file.
  base::ScopedTempDir temp_dir_;

  // This test verifies the suggestion removal only. Therefore, a mock file
  // suggest keyed service is sufficient.
  raw_ptr<MockFileSuggestKeyedService, ExperimentalAsh> file_suggest_service_ =
      nullptr;

  // The mount point for local files.
  std::unique_ptr<ScopedTestMountPoint> local_fs_mount_point_;

  // The mount point for drive files.
  std::unique_ptr<ScopedTestMountPoint> drive_fs_mount_point_;
};

// Verifies removing drive file suggestions.
TEST_F(FileSuggestKeyedServiceRemoveTest, RemoveDriveFileSuggestions) {
  const std::vector<base::FilePath> drive_file_suggestions =
      PrepareFileSuggestionsForType(FileSuggestionType::kDriveFile,
                                    /*count=*/2);
  const base::FilePath& file_path_1 = drive_file_suggestions[0];
  const base::FilePath& file_path_2 = drive_file_suggestions[1];

  absl::optional<std::vector<FileSuggestData>> suggestions =
      GetSuggestionsForType(FileSuggestionType::kDriveFile);
  EXPECT_EQ(suggestions->size(), 2u);
  EXPECT_EQ(suggestions->at(0).file_path.value(), file_path_1.value());
  EXPECT_EQ(suggestions->at(0).id, "zero_state_drive://" + file_path_1.value());
  EXPECT_EQ(suggestions->at(1).file_path.value(), file_path_2.value());
  EXPECT_EQ(suggestions->at(1).id, "zero_state_drive://" + file_path_2.value());

  MockFileSuggestKeyedServiceObserver observer_mocker;
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      scoped_observation(&observer_mocker);
  scoped_observation.Observe(file_suggest_service_.get());

  // The observer should be notified of the drive file suggestion update.
  EXPECT_CALL(observer_mocker,
              OnFileSuggestionUpdated(FileSuggestionType::kDriveFile));

  file_suggest_service_->RemoveSuggestionsAndNotify(
      /*absolute_file_paths=*/{{file_path_1}});

  // Check the suggested files after suggestion removal.
  suggestions = GetSuggestionsForType(FileSuggestionType::kDriveFile);
  EXPECT_EQ(suggestions->size(), 1u);
  EXPECT_EQ(suggestions->at(0).file_path.value(), file_path_2.value());
  EXPECT_EQ(suggestions->at(0).id, "zero_state_drive://" + file_path_2.value());
}

// Verifies removing local file suggestions.
TEST_F(FileSuggestKeyedServiceRemoveTest, RemoveLocalFileSuggestions) {
  const std::vector<base::FilePath> local_file_suggestions =
      PrepareFileSuggestionsForType(FileSuggestionType::kLocalFile,
                                    /*count=*/2);
  const base::FilePath& file_path_1 = local_file_suggestions[0];
  const base::FilePath& file_path_2 = local_file_suggestions[1];

  absl::optional<std::vector<FileSuggestData>> suggestions =
      GetSuggestionsForType(FileSuggestionType::kLocalFile);
  EXPECT_EQ(suggestions->size(), 2u);
  EXPECT_EQ(suggestions->at(0).file_path.value(), file_path_1.value());
  EXPECT_EQ(suggestions->at(0).id, "zero_state_file://" + file_path_1.value());
  EXPECT_EQ(suggestions->at(1).file_path.value(), file_path_2.value());
  EXPECT_EQ(suggestions->at(1).id, "zero_state_file://" + file_path_2.value());

  MockFileSuggestKeyedServiceObserver observer_mocker;
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      scoped_observation(&observer_mocker);
  scoped_observation.Observe(file_suggest_service_.get());

  // The observer should be notified of the local file suggestion update.
  EXPECT_CALL(observer_mocker,
              OnFileSuggestionUpdated(FileSuggestionType::kLocalFile));

  file_suggest_service_->RemoveSuggestionsAndNotify(
      /*absolute_file_paths=*/{file_path_2});

  // Check the suggested files after suggestion removal.
  suggestions = GetSuggestionsForType(FileSuggestionType::kLocalFile);
  EXPECT_EQ(suggestions->size(), 1u);
  EXPECT_EQ(suggestions->at(0).file_path.value(), file_path_1.value());
  EXPECT_EQ(suggestions->at(0).id, "zero_state_file://" + file_path_1.value());
}

// Verifies removing drive and local file suggestions at the same time.
TEST_F(FileSuggestKeyedServiceRemoveTest, RemoveMixedFileSuggestions) {
  const std::vector<base::FilePath> drive_file_suggestions =
      PrepareFileSuggestionsForType(FileSuggestionType::kDriveFile,
                                    /*count=*/1);
  const std::vector<base::FilePath> local_file_suggestions =
      PrepareFileSuggestionsForType(FileSuggestionType::kLocalFile,
                                    /*count=*/1);

  MockFileSuggestKeyedServiceObserver observer_mocker;
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      scoped_observation(&observer_mocker);
  scoped_observation.Observe(file_suggest_service_.get());

  // The observer should be notified of the updates in drive and local file
  // suggestions.
  EXPECT_CALL(observer_mocker,
              OnFileSuggestionUpdated(FileSuggestionType::kDriveFile));
  EXPECT_CALL(observer_mocker,
              OnFileSuggestionUpdated(FileSuggestionType::kLocalFile));

  file_suggest_service_->RemoveSuggestionsAndNotify(
      /*absolute_file_paths=*/{drive_file_suggestions[0],
                               local_file_suggestions[0]});

  // Check the suggested files after suggestion removal.
  EXPECT_TRUE(GetSuggestionsForType(FileSuggestionType::kDriveFile)->empty());
  EXPECT_TRUE(GetSuggestionsForType(FileSuggestionType::kLocalFile)->empty());
}

}  // namespace ash::test
