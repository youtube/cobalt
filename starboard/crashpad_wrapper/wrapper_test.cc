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

#include "starboard/crashpad_wrapper/wrapper.h"

#include <sys/time.h>

#include <csignal>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "starboard/extension/native_stability.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/minidump/minidump_file_writer.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_system_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_thread_snapshot.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

namespace crashpad {
namespace {

constexpr char kTestCrashUuid[] = "12345678-1234-1234-1234-123456789abc";
constexpr char kTestHangUuid[] = "abcdef87-4321-4321-4321-cba987654321";

class CrashpadWrapperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    internal::SetDatabasePathForTesting(temp_dir_.GetPath());
    db_ = CrashReportDatabase::Initialize(temp_dir_.GetPath());
    ASSERT_TRUE(db_);
  }

  void TearDown() override {
    internal::SetDatabasePathForTesting(base::FilePath());
  }

  base::FilePath db_path() const { return temp_dir_.GetPath(); }

  void AddReportToDatabase(const std::string& annotation_key,
                           const std::string& uuid_val,
                           int64_t timestamp_sec,
                           bool is_hang,
                           bool is_completed = false) {
    std::unique_ptr<CrashReportDatabase::NewReport> new_report;
    ASSERT_EQ(db_->PrepareNewCrashReport(&new_report),
              CrashReportDatabase::kNoError);

    test::TestProcessSnapshot snapshot;
    timeval snapshot_time{};
    snapshot_time.tv_sec = static_cast<time_t>(timestamp_sec);
    snapshot.SetSnapshotTime(snapshot_time);
    snapshot.SetAnnotationsSimpleMap({{annotation_key, uuid_val}});

    auto system_snapshot = std::make_unique<test::TestSystemSnapshot>();
    // MinidumpFileWriter requires OS, architecture, and thread CPU context to
    // be populated to serialize a valid minidump payload. Hardcoding X86_64 and
    // Linux is portable across all Evergreen architectures because Crashpad's
    // synthetic test snapshot writers compile and run on all target platforms,
    // and ReadReports only parses minidump annotations and timestamps.
    system_snapshot->SetCPUArchitecture(kCPUArchitectureX86_64);
    system_snapshot->SetOperatingSystem(SystemSnapshot::kOperatingSystemLinux);
    snapshot.SetSystem(std::move(system_snapshot));

    // Crashpad requires the exception snapshot thread ID to match an existing
    // thread in the process snapshot thread list.
    constexpr uint64_t kArbitraryThreadId = 1001;
    // A non-zero seed initializes CPU context registers with non-zero dummy
    // data to satisfy Crashpad context serialization checks.
    constexpr uint32_t kArbitraryContextSeed = 1;

    auto thread_snapshot = std::make_unique<test::TestThreadSnapshot>();
    thread_snapshot->SetThreadID(kArbitraryThreadId);
    test::InitializeCPUContextX86_64(thread_snapshot->MutableContext(),
                                     kArbitraryContextSeed);
    snapshot.AddThread(std::move(thread_snapshot));

    auto exception = std::make_unique<test::TestExceptionSnapshot>();
    exception->SetThreadID(kArbitraryThreadId);
    if (is_hang) {
      exception->SetException(
          static_cast<uint32_t>(::crashpad::Signals::kSimulatedSigno));
    } else {
      exception->SetException(SIGSEGV);
    }
    test::InitializeCPUContextX86_64(exception->MutableContext(),
                                     kArbitraryContextSeed);
    snapshot.SetException(std::move(exception));

    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&snapshot);
    ASSERT_TRUE(minidump.WriteEverything(new_report->Writer()));

    UUID uuid;
    ASSERT_EQ(db_->FinishedWritingCrashReport(std::move(new_report), &uuid),
              CrashReportDatabase::kNoError);

    if (is_completed) {
      std::unique_ptr<const CrashReportDatabase::UploadReport> upload_report;
      ASSERT_EQ(db_->GetReportForUploading(uuid, &upload_report),
                CrashReportDatabase::kNoError);
      ASSERT_EQ(db_->RecordUploadComplete(std::move(upload_report), "id_1"),
                CrashReportDatabase::kNoError);
    }
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<CrashReportDatabase> db_;
};

TEST_F(CrashpadWrapperTest, NullReportsBufferReturnsError) {
  EXPECT_EQ(ReadReports(nullptr, 1), -1);
}

TEST_F(CrashpadWrapperTest, NonPositiveMaxReportsReturnsError) {
  SbNativeStabilityReport reports[1];
  EXPECT_EQ(ReadReports(reports, 0), -1);
  EXPECT_EQ(ReadReports(reports, -1), -1);
}

TEST_F(CrashpadWrapperTest, EmptyDatabaseReturnsZero) {
  SbNativeStabilityReport reports[1];
  EXPECT_EQ(ReadReports(reports, 1), 0);
}

TEST_F(CrashpadWrapperTest, ReadsPendingCrashReport) {
  constexpr int64_t kTestTimestampSec = 100000;
  AddReportToDatabase(kNativeStabilityCrashUuidKey, kTestCrashUuid,
                      kTestTimestampSec, /*is_hang=*/false,
                      /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReports(reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportCrash);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestCrashUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(CrashpadWrapperTest, ReadsPendingHangReport) {
  constexpr int64_t kTestTimestampSec = 200000;
  AddReportToDatabase(kNativeStabilityHangUuidKey, kTestHangUuid,
                      kTestTimestampSec, /*is_hang=*/true,
                      /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReports(reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportHang);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestHangUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(CrashpadWrapperTest, ReadsCompletedCrashReport) {
  constexpr int64_t kTestTimestampSec = 300000;
  AddReportToDatabase(kNativeStabilityCrashUuidKey, kTestCrashUuid,
                      kTestTimestampSec, /*is_hang=*/false,
                      /*is_completed=*/true);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReports(reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportCrash);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestCrashUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(CrashpadWrapperTest, ReadsCompletedHangReport) {
  constexpr int64_t kTestTimestampSec = 400000;
  AddReportToDatabase(kNativeStabilityHangUuidKey, kTestHangUuid,
                      kTestTimestampSec, /*is_hang=*/true,
                      /*is_completed=*/true);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReports(reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportHang);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestHangUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(CrashpadWrapperTest, IgnoresReportWithInvalidUuidLength) {
  AddReportToDatabase(kNativeStabilityCrashUuidKey, "invalid-uuid-too-short",
                      100000, /*is_hang=*/false, /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  EXPECT_EQ(ReadReports(reports, 1), 0);
}

TEST_F(CrashpadWrapperTest, MaxNumReportsCapTruncatesOutputCount) {
  AddReportToDatabase(kNativeStabilityCrashUuidKey, kTestCrashUuid, 100000,
                      /*is_hang=*/false, /*is_completed=*/false);
  AddReportToDatabase(kNativeStabilityHangUuidKey, kTestHangUuid, 200000,
                      /*is_hang=*/true, /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  EXPECT_EQ(ReadReports(reports, 1), 1);
}

}  // namespace
}  // namespace crashpad
