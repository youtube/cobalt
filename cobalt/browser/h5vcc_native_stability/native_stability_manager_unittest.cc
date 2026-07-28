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

#include "base/functional/bind.h"
#include "base/run_loop.h"
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

}  // namespace

class NativeStabilityManagerTest : public ::testing::Test {
 protected:
  void TearDown() override {
    NativeStabilityManager::GetInstance()->ResetForTesting();
  }

  base::test::TaskEnvironment task_environment_;
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
        EXPECT_LE(reports.size(), 128u);
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

}  // namespace h5vcc_native_stability
