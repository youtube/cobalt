// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/migrate_storage_record/migration_manager.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace migrate_storage_record {

class MigrationManagerTest : public testing::Test {
 public:
  Task GroupTasks(std::vector<Task> tasks) {
    return MigrationManager::GroupTasks(std::move(tasks));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// TODO(b/399166308): Add more test cases for specific migration tasks.
TEST_F(MigrationManagerTest, VerifyGroupTasksRunsCallbacksSequentially) {
  constexpr uint32_t kTaskCount = 100;
  std::vector<Task> tasks;
  std::vector<int> collected_values;
  for (int i = 0; i < kTaskCount; i++) {
    tasks.push_back(base::BindOnce(
        [](std::vector<int>& collected_values, int i,
           base::OnceClosure callback) {
          collected_values.push_back(i);
          std::move(callback).Run();
        },
        std::ref(collected_values), i));
  }
  EXPECT_TRUE(collected_values.empty());
  Task grouped_task = GroupTasks(std::move(tasks));
  std::move(grouped_task).Run(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  std::vector<int> expected_values(kTaskCount);
  std::iota(expected_values.begin(), expected_values.end(), 0);
  EXPECT_THAT(collected_values, ::testing::ElementsAreArray(expected_values));
}

}  // namespace migrate_storage_record
}  // namespace cobalt
