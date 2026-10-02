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

#include "cobalt/memory/android_os_signal_evaluator.h"

#include <memory>
#include <vector>

#include "base/android/memory_pressure_listener_android.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace memory {

class AndroidOsSignalEvaluatorTest : public testing::Test {
 public:
  void SetUp() override {
    monitor_ =
        std::make_unique<::memory_pressure::MultiSourceMemoryPressureMonitor>();
    evaluator_ =
        std::make_unique<AndroidOsSignalEvaluator>(monitor_->CreateVoter());
  }

  void TearDown() override {
    evaluator_.reset();
    monitor_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<::memory_pressure::MultiSourceMemoryPressureMonitor> monitor_;
  std::unique_ptr<AndroidOsSignalEvaluator> evaluator_;
};

// Test to verify that AndroidOsSignalEvaluator registers its forwarder callback with
// MemoryPressureListenerAndroid on construction and unregisters on destruction.
TEST_F(AndroidOsSignalEvaluatorTest,
       RegistersForwarderAndCleansUpOnDestruction) {
  EXPECT_EQ(AndroidOsSignalEvaluator::GetInstance(), evaluator_.get());

  // Verify forwarder callback was registered.
  const auto& forwarder =
      base::android::MemoryPressureListenerAndroid::GetForwarderCallbackForTesting();
  ASSERT_TRUE(forwarder);

  // Simulate Android OS signal via forwarder.
  forwarder.Run(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(evaluator_->current_vote(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Destroy evaluator and ensure singleton instance and forwarder are cleared.
  evaluator_.reset();
  EXPECT_EQ(AndroidOsSignalEvaluator::GetInstance(), nullptr);
  EXPECT_FALSE(
      base::android::MemoryPressureListenerAndroid::GetForwarderCallbackForTesting());
}

// Test to verify that LEVEL_NONE updates the voter vote but suppresses dispatch.
TEST_F(AndroidOsSignalEvaluatorTest, NoneLevelUpdatesVoteWithoutDispatch) {
  std::vector<base::MemoryPressureListener::MemoryPressureLevel> dispatches;
  monitor_->SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel level) {
        dispatches.push_back(level);
      }));

  // CRITICAL has notify = true, so it triggers a dispatch.
  evaluator_->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(evaluator_->current_vote(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(dispatches.size(), 1u);

  // NONE updates current_vote to NONE, but notify = false suppresses dispatch.
  evaluator_->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  EXPECT_EQ(evaluator_->current_vote(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  // Verify no new dispatch occurred and that NONE was never dispatched.
  EXPECT_EQ(dispatches.size(), 1u);
  EXPECT_EQ(dispatches[0],
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace memory
}  // namespace cobalt
