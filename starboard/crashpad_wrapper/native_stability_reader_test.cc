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

#include "starboard/crashpad_wrapper/native_stability_reader.h"

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
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/minidump/minidump_file_writer.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_system_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_thread_snapshot.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

namespace crashpad {
namespace {

constexpr char kTestCrashUuid[] = "12345678-1234-1234-1234-123456789abc";
constexpr char kTestHangUuid[] = "abcdef87-4321-4321-4321-cba987654321";

class NativeStabilityReaderTest : public ::testing::Test {
 protected:
  enum class AnnotationLocation {
    kProcessSnapshot,
    kModuleSnapshot,
  };

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath db_path() const { return temp_dir_.GetPath(); }

  void AddReportToDatabase(const base::FilePath& database_path,
                           const std::string& annotation_key,
                           const std::string& uuid_val,
                           int64_t timestamp_sec,
                           bool is_hang,
                           bool is_completed = false,
                           AnnotationLocation annotation_location =
                               AnnotationLocation::kProcessSnapshot) {
    std::unique_ptr<CrashReportDatabase> db =
        CrashReportDatabase::Initialize(database_path);
    ASSERT_TRUE(db);

    std::unique_ptr<CrashReportDatabase::NewReport> new_report;
    ASSERT_EQ(db->PrepareNewCrashReport(&new_report),
              CrashReportDatabase::kNoError);

    test::TestProcessSnapshot snapshot;
    timeval snapshot_time{};
    snapshot_time.tv_sec = static_cast<time_t>(timestamp_sec);
    snapshot.SetSnapshotTime(snapshot_time);
    if (annotation_location == AnnotationLocation::kModuleSnapshot) {
      auto module_snapshot = std::make_unique<test::TestModuleSnapshot>();
      module_snapshot->SetName("libchrobalt.so");
      std::vector<uint8_t> value_bytes(uuid_val.begin(), uuid_val.end());
      module_snapshot->SetAnnotationObjects({AnnotationSnapshot(
          annotation_key, static_cast<uint16_t>(Annotation::Type::kString),
          value_bytes)});
      snapshot.AddModule(std::move(module_snapshot));
    } else {
      snapshot.SetAnnotationsSimpleMap({{annotation_key, uuid_val}});
    }

    auto system_snapshot = std::make_unique<test::TestSystemSnapshot>();
    // MinidumpFileWriter requires OS, architecture, and thread CPU context to
    // be populated to serialize a valid minidump payload. Hardcoding X86_64 and
    // Linux is portable across all architectures because Crashpad's synthetic
    // test snapshot writers compile and run on all target platforms, and
    // ReadReportsFromDatabase only parses minidump annotations and timestamps.
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
    ASSERT_EQ(db->FinishedWritingCrashReport(std::move(new_report), &uuid),
              CrashReportDatabase::kNoError);

    if (is_completed) {
      std::unique_ptr<const CrashReportDatabase::UploadReport> upload_report;
      ASSERT_EQ(db->GetReportForUploading(uuid, &upload_report),
                CrashReportDatabase::kNoError);
      ASSERT_EQ(db->RecordUploadComplete(std::move(upload_report), "id_1"),
                CrashReportDatabase::kNoError);
    }
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(NativeStabilityReaderTest, NullReportsBufferReturnsError) {
  EXPECT_EQ(ReadReportsFromDatabase(db_path(), nullptr, 1), -1);
}

TEST_F(NativeStabilityReaderTest, NonPositiveMaxReportsReturnsError) {
  SbNativeStabilityReport reports[1];
  EXPECT_EQ(ReadReportsFromDatabase(db_path(), reports, 0), -1);
  EXPECT_EQ(ReadReportsFromDatabase(db_path(), reports, -1), -1);
}

TEST_F(NativeStabilityReaderTest, NoDatabaseAtPathReturnsZero) {
  SbNativeStabilityReport reports[1];
  EXPECT_EQ(ReadReportsFromDatabase(db_path().AppendASCII("nonexistent_db"),
                                    reports, 1),
            0);
}

TEST_F(NativeStabilityReaderTest, EmptyDatabaseReturnsZero) {
  ASSERT_TRUE(CrashReportDatabase::Initialize(db_path()));
  SbNativeStabilityReport reports[1];
  EXPECT_EQ(ReadReportsFromDatabase(db_path(), reports, 1), 0);
}

TEST_F(NativeStabilityReaderTest, ReadsPendingCrashReport) {
  constexpr int64_t kTestTimestampSec = 100000;
  AddReportToDatabase(db_path(), kNativeStabilityCrashUuidKey, kTestCrashUuid,
                      kTestTimestampSec, /*is_hang=*/false,
                      /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReportsFromDatabase(db_path(), reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportCrash);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestCrashUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(NativeStabilityReaderTest, ReadsPendingHangReport) {
  constexpr int64_t kTestTimestampSec = 200000;
  AddReportToDatabase(db_path(), kNativeStabilityHangUuidKey, kTestHangUuid,
                      kTestTimestampSec, /*is_hang=*/true,
                      /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReportsFromDatabase(db_path(), reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportHang);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestHangUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(NativeStabilityReaderTest, ReadsCompletedCrashReport) {
  constexpr int64_t kTestTimestampSec = 300000;
  AddReportToDatabase(db_path(), kNativeStabilityCrashUuidKey, kTestCrashUuid,
                      kTestTimestampSec, /*is_hang=*/false,
                      /*is_completed=*/true);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReportsFromDatabase(db_path(), reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportCrash);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestCrashUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(NativeStabilityReaderTest, ReadsCompletedHangReport) {
  constexpr int64_t kTestTimestampSec = 400000;
  AddReportToDatabase(db_path(), kNativeStabilityHangUuidKey, kTestHangUuid,
                      kTestTimestampSec, /*is_hang=*/true,
                      /*is_completed=*/true);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReportsFromDatabase(db_path(), reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportHang);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestHangUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(NativeStabilityReaderTest, ReadsCompletedHangReportFromModuleSnapshot) {
  constexpr int64_t kTestTimestampSec = 500000;
  AddReportToDatabase(db_path(), kNativeStabilityHangUuidKey, kTestHangUuid,
                      kTestTimestampSec, /*is_hang=*/true,
                      /*is_completed=*/true,
                      AnnotationLocation::kModuleSnapshot);

  SbNativeStabilityReport reports[1]{};
  int count = ReadReportsFromDatabase(db_path(), reports, 1);
  EXPECT_EQ(count, 1);
  EXPECT_EQ(reports[0].report_type, kSbNativeStabilityReportHang);
  EXPECT_STREQ(reports[0].native_stability_event_uuid, kTestHangUuid);
  EXPECT_EQ(reports[0].event_time_s, kTestTimestampSec);
}

TEST_F(NativeStabilityReaderTest, IgnoresReportWithInvalidUuidLength) {
  AddReportToDatabase(db_path(), kNativeStabilityCrashUuidKey,
                      "invalid-uuid-too-short", 100000, /*is_hang=*/false,
                      /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  EXPECT_EQ(ReadReportsFromDatabase(db_path(), reports, 1), 0);
}

TEST_F(NativeStabilityReaderTest, MaxNumReportsCapTruncatesOutputCount) {
  AddReportToDatabase(db_path(), kNativeStabilityCrashUuidKey, kTestCrashUuid,
                      100000, /*is_hang=*/false, /*is_completed=*/false);
  AddReportToDatabase(db_path(), kNativeStabilityHangUuidKey, kTestHangUuid,
                      200000, /*is_hang=*/true, /*is_completed=*/false);

  SbNativeStabilityReport reports[1]{};
  EXPECT_EQ(ReadReportsFromDatabase(db_path(), reports, 1), 1);
}

}  // namespace
}  // namespace crashpad
