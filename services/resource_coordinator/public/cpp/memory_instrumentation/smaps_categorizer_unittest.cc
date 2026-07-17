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

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace memory_instrumentation {

namespace {

class MockDetailedMetricsDelegate : public DetailedMetricsDelegate {
 public:
  MOCK_METHOD(void, OnSmapsEntry, (absl::string_view name, const SmapsMetrics& metrics), (override));
  MOCK_METHOD(void, GetAndResetStats, ((base::flat_map<std::string, uint64_t>* stats)), (override));
  
  base::WeakPtr<DetailedMetricsDelegate> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDetailedMetricsDelegate> weak_ptr_factory_{this};
};

const char kTestSmaps[] =
    "00400000-004be000 r-xp 00000000 fc:01 1234              /file/1\n"
    "Size:                760 kB\n"
    "Rss:                 296 kB\n"
    "Pss:                 162 kB\n"
    "Swap:                  4 kB\n"
    "7fe7ce79c000-7fe7ce7a8000 ---p 00000000 00:00 0 \n"
    "Size:                 48 kB\n"
    "Rss:                  40 kB\n"
    "Pss:                  32 kB\n";

}  // namespace

class SmapsCategorizerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SmapsCategorizerTest, ScanSmapsFile) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath smaps_path = temp_dir_.GetPath().Append("smaps");
  ASSERT_TRUE(base::WriteFile(smaps_path, kTestSmaps));

  auto results = SmapsCategorizer::ScanSmapsFile(smaps_path);
  ASSERT_TRUE(results.has_value());
  ASSERT_EQ(results->size(), 2u);

  EXPECT_EQ((*results)[0].name, "/file/1");
  EXPECT_EQ((*results)[0].metrics.rss_kb, 296u);
  EXPECT_EQ((*results)[0].metrics.pss_kb, 162u);
  EXPECT_EQ((*results)[0].metrics.swap_kb, 4u);

  EXPECT_EQ((*results)[1].name, "");
  EXPECT_EQ((*results)[1].metrics.rss_kb, 40u);
  EXPECT_EQ((*results)[1].metrics.pss_kb, 32u);
}

TEST_F(SmapsCategorizerTest, RequestDumpCoalescing) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  
  testing::NiceMock<MockDetailedMetricsDelegate> delegate;
  SmapsCategorizer categorizer(&delegate);

  base::RunLoop run_loop;
  int call_count = 0;
  auto callback = base::BindRepeating([](int* count, base::RunLoop* run_loop) {
    (*count)++;
    if (*count == 3) {
      run_loop->Quit();
    }
  }, &call_count, &run_loop);

  categorizer.RequestDump(base::BindOnce(callback));
  categorizer.RequestDump(base::BindOnce(callback));
  categorizer.RequestDump(base::BindOnce(callback));

  run_loop.Run();

  EXPECT_EQ(call_count, 3);
}

}  // namespace memory_instrumentation
