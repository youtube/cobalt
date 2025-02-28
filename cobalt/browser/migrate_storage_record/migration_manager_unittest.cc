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

namespace cobalt {
namespace migrate_storage_record {

class MigrationManagerTest : public testing::Test {
 public:
  void DoTasks(std::vector<Task> tasks) {
    MigrationManager::DoTasks(std::move(tasks));
  }

  Task GroupTasks(std::vector<Task> tasks) {
    return MigrationManager::GroupTasks(std::move(tasks));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MigrationManagerTest, DoTasksTest) {
  std::vector<Task> tasks;
  std::vector<int> items;
  for (int i = 0; i < 100; i++) {
    tasks.push_back(base::BindOnce(
        [](std::vector<int>& items, int i, base::OnceClosure callback) {
          items.push_back(i);
          std::move(callback).Run();
        },
        std::ref(items), i));
  }
  EXPECT_TRUE(items.empty());
  DoTasks(std::move(tasks));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(100, items.size());
  EXPECT_EQ(0, items[0]);
  EXPECT_EQ(99, items[99]);
}

TEST_F(MigrationManagerTest, GroupTasksTest) {
  std::vector<Task> tasks;
  std::vector<int> items;
  for (int i = 0; i < 100; i++) {
    tasks.push_back(base::BindOnce(
        [](std::vector<int>& items, int i, base::OnceClosure callback) {
          items.push_back(i);
          std::move(callback).Run();
        },
        std::ref(items), i));
  }
  EXPECT_TRUE(items.empty());
  Task grouped_task = GroupTasks(std::move(tasks));
  std::move(grouped_task).Run(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(100, items.size());
  EXPECT_EQ(0, items[0]);
  EXPECT_EQ(99, items[99]);
}

}  // namespace migrate_storage_record
}  // namespace cobalt
