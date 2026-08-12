// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/h5vcc_native_stability/native_stability_manager.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/h5vcc_native_stability/public/mojom/h5vcc_native_stability.mojom.h"
#include "starboard/extension/native_stability.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace h5vcc_native_stability {

namespace {

void SetupStubExtension(
    NativeStabilityManager* manager,
    const std::vector<SbNativeStabilityReport>& reports_to_return) {
  static std::vector<SbNativeStabilityReport> s_reports;
  s_reports = reports_to_return;

  static StarboardExtensionNativeStabilityApi s_api = {
      kStarboardExtensionNativeStabilityName,
      1,
      [](SbNativeStabilityReport* reports, int max_reports) -> int {
        int count = std::min(static_cast<int>(s_reports.size()), max_reports);
        for (int i = 0; i < count; ++i) {
          reports[i] = s_reports[i];
        }
        return count;
      },
  };

  manager->SetGetExtensionForTesting(
      base::BindRepeating([](const char* name) -> const void* {
        if (std::strcmp(name, kStarboardExtensionNativeStabilityName) == 0) {
          return &s_api;
        }
        return nullptr;
      }));
}

std::unordered_set<std::string> ReadAckedUuidsFromDiskForTesting(
    const base::FilePath& file_path) {
  std::unordered_set<std::string> acked_uuids;
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    return acked_uuids;
  }
  std::optional<base::Value::List> parsed_list =
      base::JSONReader::ReadList(file_content);
  if (!parsed_list) {
    return acked_uuids;
  }
  for (const auto& item : *parsed_list) {
    if (item.is_string()) {
      acked_uuids.insert(item.GetString());
    }
  }
  return acked_uuids;
}

}  // namespace

class NativeStabilityManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Overriding the acked UUIDs file path with a unique temporary directory
    // for every test 1) isolates test storage and 2) ensures actual platform
    // directories (e.g. kSbSystemPathCacheDirectory) are not used.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    NativeStabilityManager::GetInstance()->SetAckedUuidsFilePathForTesting(
        temp_dir_.GetPath().Append("acked_event_uuids.json"));
  }

  void TearDown() override {
    NativeStabilityManager::GetInstance()->ResetForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(NativeStabilityManagerTest, GetInstanceReturnsSingleton) {
  auto* manager1 = NativeStabilityManager::GetInstance();
  auto* manager2 = NativeStabilityManager::GetInstance();
  EXPECT_NE(manager1, nullptr);
  EXPECT_EQ(manager1, manager2);
}

TEST_F(NativeStabilityManagerTest,
       GetPendingReportsReturnsEmptyWhenExtensionNotImplemented) {
  auto* manager = NativeStabilityManager::GetInstance();
  manager->SetGetExtensionForTesting(base::BindRepeating(
      [](const char* name) -> const void* { return nullptr; }));

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        EXPECT_TRUE(reports.empty());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest,
       GetPendingReportsReturnsEmptyWhenExtensionReturnsNoReports) {
  auto* manager = NativeStabilityManager::GetInstance();
  SetupStubExtension(manager, {});

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        EXPECT_TRUE(reports.empty());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest, GetPendingReportsParsesCrashReport) {
  const std::string kExpectedUuid = "crash-uuid-12345";
  const int64_t kExpectedTime = 1700000000;

  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport stub_report = {};
  stub_report.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(stub_report.native_stability_event_uuid, kExpectedUuid.c_str(),
               sizeof(stub_report.native_stability_event_uuid) - 1);
  stub_report.event_time_s = kExpectedTime;

  SetupStubExtension(manager, {stub_report});

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](const std::string& expected_uuid, int64_t expected_time,
         base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        ASSERT_EQ(reports.size(), 1u);
        ASSERT_TRUE(reports[0]->is_crash_report());
        const auto& crash = reports[0]->get_crash_report();
        ASSERT_TRUE(crash->base);
        EXPECT_EQ(crash->base->native_stability_event_uuid, expected_uuid);
        EXPECT_EQ(crash->base->event_time_sec, expected_time);
        std::move(quit_closure).Run();
      },
      kExpectedUuid, kExpectedTime, run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest, GetPendingReportsParsesHangReport) {
  const std::string kExpectedUuid = "hang-uuid-67890";
  const int64_t kExpectedTime = 1700005000;

  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport stub_report = {};
  stub_report.report_type = kSbNativeStabilityReportHang;
  std::strncpy(stub_report.native_stability_event_uuid, kExpectedUuid.c_str(),
               sizeof(stub_report.native_stability_event_uuid) - 1);
  stub_report.event_time_s = kExpectedTime;

  SetupStubExtension(manager, {stub_report});

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](const std::string& expected_uuid, int64_t expected_time,
         base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        ASSERT_EQ(reports.size(), 1u);
        ASSERT_TRUE(reports[0]->is_hang_report());
        const auto& hang = reports[0]->get_hang_report();
        ASSERT_TRUE(hang->base);
        EXPECT_EQ(hang->base->native_stability_event_uuid, expected_uuid);
        EXPECT_EQ(hang->base->event_time_sec, expected_time);
        EXPECT_FALSE(hang->is_recovered);
        std::move(quit_closure).Run();
      },
      kExpectedUuid, kExpectedTime, run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest,
       GetPendingReportsClampsOutOfBoundsExtensionCount) {
  auto* manager = NativeStabilityManager::GetInstance();

  static StarboardExtensionNativeStabilityApi s_violating_api = {
      kStarboardExtensionNativeStabilityName,
      1,
      [](SbNativeStabilityReport* reports, int max_reports) -> int {
        for (int i = 0; i < max_reports; ++i) {
          reports[i].report_type = kSbNativeStabilityReportCrash;
          std::string uuid = base::StringPrintf("crash-uuid-%05d", i);
          std::strncpy(reports[i].native_stability_event_uuid, uuid.c_str(),
                       sizeof(reports[i].native_stability_event_uuid) - 1);
        }
        return 999;  // Return out-of-bounds count exceeding max_reports
      },
  };

  manager->SetGetExtensionForTesting(
      base::BindRepeating([](const char* name) -> const void* {
        if (std::strcmp(name, kStarboardExtensionNativeStabilityName) == 0) {
          return &s_violating_api;
        }
        return nullptr;
      }));

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        // Verify no crash occurred and count was clamped to kMaxNumReports
        // (128)
        EXPECT_EQ(reports.size(), 128u);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest,
       GetPendingReportsHandlesNegativeExtensionCount) {
  auto* manager = NativeStabilityManager::GetInstance();

  static StarboardExtensionNativeStabilityApi s_negative_count_api = {
      kStarboardExtensionNativeStabilityName,
      1,
      [](SbNativeStabilityReport* reports, int max_reports) -> int {
        return -1;  // Return invalid negative count
      },
  };

  manager->SetGetExtensionForTesting(
      base::BindRepeating([](const char* name) -> const void* {
        if (std::strcmp(name, kStarboardExtensionNativeStabilityName) == 0) {
          return &s_negative_count_api;
        }
        return nullptr;
      }));

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        // Verify empty results vector is returned on negative count
        EXPECT_TRUE(reports.empty());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest, GetPendingReportsIgnoresUnknownReportType) {
  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport unknown_report = {};
  unknown_report.report_type = kSbNativeStabilityReportUnknown;
  std::strncpy(unknown_report.native_stability_event_uuid, "unknown-uuid-12345",
               sizeof(unknown_report.native_stability_event_uuid) - 1);
  unknown_report.event_time_s = 1700000000;

  SetupStubExtension(manager, {unknown_report});

  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        // Verify unknown report type was skipped and not included in results
        EXPECT_TRUE(reports.empty());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest,
       AcknowledgedReportsFilteredByGetPendingReports) {
  auto* manager = NativeStabilityManager::GetInstance();

  SbNativeStabilityReport report1 = {};
  report1.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report1.native_stability_event_uuid, "uuid-1",
               sizeof(report1.native_stability_event_uuid) - 1);
  report1.event_time_s = 1000;

  SbNativeStabilityReport report2 = {};
  report2.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report2.native_stability_event_uuid, "uuid-2",
               sizeof(report2.native_stability_event_uuid) - 1);
  report2.event_time_s = 2000;

  SetupStubExtension(manager, {report1, report2});

  // Verify initial call returns both reports.
  {
    base::RunLoop run_loop;
    manager->GetPendingReports(base::BindOnce(
        [](base::OnceClosure quit_closure,
           std::vector<mojom::NativeStabilityReportPtr> reports) {
          ASSERT_EQ(reports.size(), 2u);
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Acknowledge uuid-1.
  {
    base::RunLoop run_loop;
    manager->AcknowledgeReports({"uuid-1"}, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify subsequent call returns only uuid-2.
  {
    base::RunLoop run_loop;
    manager->GetPendingReports(base::BindOnce(
        [](base::OnceClosure quit_closure,
           std::vector<mojom::NativeStabilityReportPtr> reports) {
          ASSERT_EQ(reports.size(), 1u);
          EXPECT_EQ(
              reports[0]->get_crash_report()->base->native_stability_event_uuid,
              "uuid-2");
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Acknowledge uuid-2 as well.
  {
    base::RunLoop run_loop;
    manager->AcknowledgeReports({"uuid-2"}, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify GetPendingReports returns no reports once all are acknowledged.
  {
    base::RunLoop run_loop;
    manager->GetPendingReports(base::BindOnce(
        [](base::OnceClosure quit_closure,
           std::vector<mojom::NativeStabilityReportPtr> reports) {
          EXPECT_TRUE(reports.empty());
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(NativeStabilityManagerTest,
       AcknowledgingEmptyOrDuplicateUuidsHasNoEffect) {
  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport report1 = {};
  report1.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report1.native_stability_event_uuid, "uuid-1",
               sizeof(report1.native_stability_event_uuid) - 1);
  SetupStubExtension(manager, {report1});

  // Acknowledge empty strings and duplicates alongside valid UUID.
  base::RunLoop run_loop;
  manager->AcknowledgeReports({"", "uuid-1", "uuid-1", ""},
                              run_loop.QuitClosure());
  run_loop.Run();

  // Verify uuid-1 was acknowledged and no crash occurred.
  base::RunLoop run_loop2;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        EXPECT_TRUE(reports.empty());
        std::move(quit_closure).Run();
      },
      run_loop2.QuitClosure()));
  run_loop2.Run();
}

TEST_F(NativeStabilityManagerTest,
       GetPendingReportsIgnoresCorruptedAckedUuidsFile) {
  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport report1 = {};
  report1.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report1.native_stability_event_uuid, "uuid-1",
               sizeof(report1.native_stability_event_uuid) - 1);
  SetupStubExtension(manager, {report1});

  // Write corrupted JSON to the acked UUIDs file path.
  base::FilePath file_path =
      temp_dir_.GetPath().Append("acked_event_uuids.json");
  ASSERT_TRUE(base::WriteFile(file_path, "invalid json"));

  // Verify GetPendingReports handles corrupt JSON gracefully and returns the
  // pending report.
  base::RunLoop run_loop;
  manager->GetPendingReports(base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        EXPECT_EQ(reports.size(), 1u);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(NativeStabilityManagerTest, PruneStorageRemovesObsoleteAckedUuids) {
  auto* manager = NativeStabilityManager::GetInstance();

  base::FilePath file_path =
      temp_dir_.GetPath().Append("acked_event_uuids.json");

  {
    base::RunLoop run_loop;
    manager->AcknowledgeReports({"uuid-1", "uuid-2", "uuid-3"},
                                run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1", "uuid-2", "uuid-3"}));

  // Configure extension to only return report1 (simulating reports for uuid-2
  // and uuid-3 having been pruned from local crash storage).
  SbNativeStabilityReport report1 = {};
  report1.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report1.native_stability_event_uuid, "uuid-1",
               sizeof(report1.native_stability_event_uuid) - 1);
  SetupStubExtension(manager, {report1});

  // Prune storage against the updated crash storage state.
  {
    base::RunLoop run_loop;
    manager->PruneStorage(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify directly on disk that obsolete UUIDs ("uuid-2", "uuid-3") were
  // removed, leaving only "uuid-1".
  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));
}

TEST_F(NativeStabilityManagerTest,
       PruneStorageDoesNothingWhenExtensionNotImplemented) {
  auto* manager = NativeStabilityManager::GetInstance();

  base::FilePath file_path =
      temp_dir_.GetPath().Append("acked_event_uuids.json");

  {
    base::RunLoop run_loop;
    manager->AcknowledgeReports({"uuid-1"}, run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));

  // Disable extension.
  manager->SetGetExtensionForTesting(base::BindRepeating(
      [](const char* name) -> const void* { return nullptr; }));

  {
    base::RunLoop run_loop;
    manager->PruneStorage(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify acked_event_uuids.json still contains exactly uuid-1.
  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));
}

TEST_F(NativeStabilityManagerTest,
       PruneStorageDoesNothingWhenAllAckedUuidsAreStillPersisted) {
  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport report1 = {};
  report1.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report1.native_stability_event_uuid, "uuid-1",
               sizeof(report1.native_stability_event_uuid) - 1);

  SbNativeStabilityReport report2 = {};
  report2.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report2.native_stability_event_uuid, "uuid-2",
               sizeof(report2.native_stability_event_uuid) - 1);

  // Simulate persistence of both reports.
  SetupStubExtension(manager, {report1, report2});

  base::FilePath file_path =
      temp_dir_.GetPath().Append("acked_event_uuids.json");

  {
    base::RunLoop run_loop;
    manager->AcknowledgeReports({"uuid-1"}, run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));

  {
    base::RunLoop run_loop;
    manager->PruneStorage(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify acked_event_uuids.json still contains exactly uuid-1.
  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));
}

TEST_F(NativeStabilityManagerTest,
       PruneStorageDoesNothingWhenExtensionReturnsError) {
  auto* manager = NativeStabilityManager::GetInstance();

  base::FilePath file_path =
      temp_dir_.GetPath().Append("acked_event_uuids.json");

  {
    base::RunLoop run_loop;
    manager->AcknowledgeReports({"uuid-1"}, run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));

  static StarboardExtensionNativeStabilityApi s_error_api = {
      kStarboardExtensionNativeStabilityName,
      1,
      [](SbNativeStabilityReport* reports, int max_reports) -> int {
        return -1;
      },
  };

  manager->SetGetExtensionForTesting(
      base::BindRepeating([](const char* name) -> const void* {
        if (std::strcmp(name, kStarboardExtensionNativeStabilityName) == 0) {
          return &s_error_api;
        }
        return nullptr;
      }));

  {
    base::RunLoop run_loop;
    manager->PruneStorage(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify acked_event_uuids.json still contains exactly uuid-1.
  EXPECT_EQ(ReadAckedUuidsFromDiskForTesting(file_path),
            (std::unordered_set<std::string>{"uuid-1"}));
}

TEST_F(NativeStabilityManagerTest,
       PruneStorageDoesNothingWhenAckedUuidsFileDoesNotExist) {
  auto* manager = NativeStabilityManager::GetInstance();
  SbNativeStabilityReport report1 = {};
  report1.report_type = kSbNativeStabilityReportCrash;
  std::strncpy(report1.native_stability_event_uuid, "uuid-1",
               sizeof(report1.native_stability_event_uuid) - 1);

  SetupStubExtension(manager, {report1});

  base::FilePath file_path =
      temp_dir_.GetPath().Append("acked_event_uuids.json");
  ASSERT_FALSE(base::PathExists(file_path));

  // Prune storage when acked_event_uuids.json does not exist.
  {
    base::RunLoop run_loop;
    manager->PruneStorage(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify file was not created on disk.
  EXPECT_FALSE(base::PathExists(file_path));
}

}  // namespace h5vcc_native_stability
