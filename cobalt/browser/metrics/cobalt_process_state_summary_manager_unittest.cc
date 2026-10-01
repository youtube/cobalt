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

#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

#include <vector>

#include "base/hash/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class CobaltProcessStateSummaryManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    manager_ = CobaltProcessStateSummaryManager::GetInstance();
    manager_->ResetForTesting();
  }

  void TearDown() override { manager_->ResetForTesting(); }

  CobaltProcessStateSummaryManager* manager_;
};

TEST_F(CobaltProcessStateSummaryManagerTest,
       SerializeAndDeserialize_RoundTrip) {
  ProcessStateSummaryData original;
  original.startup_guard_armed = true;
  original.startup_guard_triggered_kill = false;
  original.startup_milestones = (1ULL << 1) | (1ULL << 5) | (1ULL << 37);
  original.highest_milestone = 37;

  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(original);
  ASSERT_EQ(serialized.size(),
            CobaltProcessStateSummaryManager::kSummaryPayloadSize);
  EXPECT_EQ(serialized[0], CobaltProcessStateSummaryManager::kMagicByte);
  EXPECT_EQ(serialized[1], CobaltProcessStateSummaryManager::kVersionByte);
  EXPECT_EQ(serialized[2],
            CobaltProcessStateSummaryManager::kFlagStartupGuardArmed);
  EXPECT_EQ(serialized[11], 37);

  auto deserialized = CobaltProcessStateSummaryManager::Deserialize(serialized);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SerializeAndDeserialize_TriggeredKill) {
  ProcessStateSummaryData original;
  original.startup_guard_armed = false;
  original.startup_guard_triggered_kill = true;
  original.startup_milestones = (1ULL << 2) | (1ULL << 10);
  original.highest_milestone = 10;

  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(original);
  EXPECT_EQ(serialized[2],
            CobaltProcessStateSummaryManager::kFlagStartupGuardTriggeredKill);

  auto deserialized = CobaltProcessStateSummaryManager::Deserialize(serialized);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_InvalidMagic) {
  ProcessStateSummaryData data;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);
  serialized[0] = 0x00;  // Corrupt magic

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_InvalidVersion) {
  ProcessStateSummaryData data;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);
  serialized[1] = 0x02;  // Corrupt version

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_CorruptedChecksum) {
  ProcessStateSummaryData data;
  data.highest_milestone = 15;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);

  // Mutate one byte in data payload
  serialized[11] = 16;

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_TruncatedData) {
  ProcessStateSummaryData data;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);
  serialized.resize(15);  // Truncated to 15 bytes

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SetStartupMilestone_UpdatesBitmaskAndHighest) {
  manager_->SetStartupMilestone(1);
  manager_->SetStartupMilestone(5);
  manager_->SetStartupMilestone(37);

  ProcessStateSummaryData current = manager_->GetCurrentDataForTesting();
  EXPECT_EQ(current.startup_milestones,
            (1ULL << 1) | (1ULL << 5) | (1ULL << 37));
  EXPECT_EQ(current.highest_milestone, 37);

  // Duplicate milestone should be idempotent
  manager_->SetStartupMilestone(5);
  current = manager_->GetCurrentDataForTesting();
  EXPECT_EQ(current.startup_milestones,
            (1ULL << 1) | (1ULL << 5) | (1ULL << 37));
  EXPECT_EQ(current.highest_milestone, 37);

  // Lower milestone should not decrease highest_milestone
  manager_->SetStartupMilestone(10);
  current = manager_->GetCurrentDataForTesting();
  EXPECT_EQ(current.startup_milestones,
            (1ULL << 1) | (1ULL << 5) | (1ULL << 10) | (1ULL << 37));
  EXPECT_EQ(current.highest_milestone, 37);
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SetStartupGuardArmed_UpdatesArmedFlag) {
  manager_->SetStartupGuardArmed(true);
  EXPECT_TRUE(manager_->GetCurrentDataForTesting().startup_guard_armed);

  manager_->SetStartupGuardArmed(false);
  EXPECT_FALSE(manager_->GetCurrentDataForTesting().startup_guard_armed);
}

}  // namespace
}  // namespace cobalt
