// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_UNITTEST_CC_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_UNITTEST_CC_

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using ::testing::_;
using ::testing::Invoke;

class MockDetailedMetricsDelegate : public DetailedMetricsDelegate {
 public:
  MockDetailedMetricsDelegate() = default;
  ~MockDetailedMetricsDelegate() override = default;

  MOCK_METHOD(void, OnSmapsHeader, (absl::string_view line), (override));
  MOCK_METHOD(void, OnSmapsCounter, (absl::string_view name, uint64_t value_kb), (override));
  MOCK_METHOD((base::flat_map<std::string, uint32_t>), GetAndResetStats, (), (override));
};

class SmapsCategorizerTest : public testing::Test {
 public:
  SmapsCategorizerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  MockDetailedMetricsDelegate delegate_;
};

TEST_F(SmapsCategorizerTest, BasicParsing) {
  SmapsCategorizer categorizer(&delegate_);
  std::string smaps_content =
      "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
      "Size:                132 kB\n"
      "Pss:                  40 kB\n"
      "Rss:                  60 kB\n"
      "Referenced:           40 kB\n"
      "00421000-00422000 r--p 00021000 fc:01 1234  /foo.so\n"
      "Size:                  4 kB\n"
      "Pss:                   4 kB\n"
      "Rss:                   4 kB\n";

  std::vector<char> buffer(smaps_content.begin(), smaps_content.end());
  mojom::RawOSMemDumpPtr dump = mojom::RawOSMemDump::New();
  dump->pss_kb = 44; // Total Pss in smaps is 40 + 4 = 44.

  EXPECT_CALL(delegate_, OnSmapsHeader("00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so")).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Size", 132)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Pss", 40)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Rss", 60)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Referenced", 40)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsHeader("00421000-00422000 r--p 00021000 fc:01 1234  /foo.so")).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Size", 4)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Pss", 4)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Rss", 4)).Times(1);

  base::flat_map<std::string, uint32_t> expected_stats = {{"pss:foo", 44}};
  EXPECT_CALL(delegate_, GetAndResetStats()).WillOnce(Invoke([&]() {
    return expected_stats;
  }));

  base::RunLoop run_loop;
  categorizer.Start(std::move(buffer), dump.get(),
                    base::BindOnce([](base::RunLoop* run_loop, bool success) {
                      EXPECT_TRUE(success);
                      run_loop->Quit();
                    }, &run_loop));
  run_loop.Run();

  EXPECT_EQ(dump->detailed_stats_kb, expected_stats);
  EXPECT_FALSE(dump->last_detailed_dump_time.is_null());
}

TEST_F(SmapsCategorizerTest, ValidationFailure) {
  SmapsCategorizer categorizer(&delegate_);
  std::string smaps_content =
      "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
      "Pss:                  40 kB\n";

  std::vector<char> buffer(smaps_content.begin(), smaps_content.end());
  mojom::RawOSMemDumpPtr dump = mojom::RawOSMemDump::New();
  dump->pss_kb = 100; // Large discrepancy: 40 vs 100.

  EXPECT_CALL(delegate_, OnSmapsHeader(_)).Times(1);
  EXPECT_CALL(delegate_, OnSmapsCounter("Pss", 40)).Times(1);
  
  // Stats should be reset but NOT populated to dump on validation failure.
  EXPECT_CALL(delegate_, GetAndResetStats()).Times(1);

  base::RunLoop run_loop;
  categorizer.Start(std::move(buffer), dump.get(),
                    base::BindOnce([](base::RunLoop* run_loop, bool success) {
                      EXPECT_TRUE(success);
                      run_loop->Quit();
                    }, &run_loop));
  run_loop.Run();

  EXPECT_TRUE(dump->detailed_stats_kb.empty());
  EXPECT_TRUE(dump->last_detailed_dump_time.is_null());
}

TEST_F(SmapsCategorizerTest, ConcurrentStart) {
  SmapsCategorizer categorizer(&delegate_);
  
  std::string smaps_1 = "Pss: 10 kB\n";
  std::string smaps_2 = "Pss: 20 kB\n";
  
  mojom::RawOSMemDumpPtr dump_1 = mojom::RawOSMemDump::New();
  mojom::RawOSMemDumpPtr dump_2 = mojom::RawOSMemDump::New();
  
  bool callback_1_run = false;
  bool callback_1_success = true;
  bool callback_2_run = false;
  
  categorizer.Start(std::vector<char>(smaps_1.begin(), smaps_1.end()), 
                    dump_1.get(),
                    base::BindOnce([](bool* run, bool* success, bool res) {
                      *run = true;
                      *success = res;
                    }, &callback_1_run, &callback_1_success));
  
  // Immediately start another one.
  categorizer.Start(std::vector<char>(smaps_2.begin(), smaps_2.end()), 
                    dump_2.get(),
                    base::BindOnce([](bool* run, bool res) {
                      *run = true;
                    }, &callback_2_run));
  
  EXPECT_TRUE(callback_1_run);
  EXPECT_FALSE(callback_1_success); // Cancelled
  EXPECT_FALSE(callback_2_run); // Still pending in task runner
  
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_2_run);
}

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_UNITTEST_CC_
