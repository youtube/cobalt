// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy.h"

#include <memory>

#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

class MockWorkingSetTrimmerPolicy : public WorkingSetTrimmerPolicy {
 public:
  MockWorkingSetTrimmerPolicy() {}

  MockWorkingSetTrimmerPolicy(const MockWorkingSetTrimmerPolicy&) = delete;
  MockWorkingSetTrimmerPolicy& operator=(const MockWorkingSetTrimmerPolicy&) =
      delete;

  ~MockWorkingSetTrimmerPolicy() override {}

  MOCK_METHOD1(TrimWorkingSet, void(const ProcessNode*));
};

class WorkingSetTrimmerPolicyTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  WorkingSetTrimmerPolicyTest() {}

  WorkingSetTrimmerPolicyTest(const WorkingSetTrimmerPolicyTest&) = delete;
  WorkingSetTrimmerPolicyTest& operator=(const WorkingSetTrimmerPolicyTest&) =
      delete;

  ~WorkingSetTrimmerPolicyTest() override {}

  void SetUp() override {
    Super::SetUp();
    policy_ = std::make_unique<WorkingSetTrimmerPolicy>();
  }

  void SetLastTrimTime(const ProcessNode* node, base::TimeTicks time) {
    policy_->SetLastTrimTime(node, time);
  }

  base::TimeTicks GetLastTrimTime(const ProcessNode* node) {
    return policy_->GetLastTrimTime(node);
  }

 protected:
  std::unique_ptr<WorkingSetTrimmerPolicy> policy_;
};

// Validate that we can set and get the last trim time on a ProcessNode.
TEST_F(WorkingSetTrimmerPolicyTest, SetTrimTimeOnNode) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  ASSERT_NE(process_node.get(), nullptr);

  auto now_ticks = base::TimeTicks::Now();
  SetLastTrimTime(process_node.get(), now_ticks);
  ASSERT_EQ(GetLastTrimTime(process_node.get()), now_ticks);
}

// Validate that when all frames in a ProcessNode are frozen we attempt to trim
// the working set.
TEST_F(WorkingSetTrimmerPolicyTest, TrimOnFrozen) {
  std::unique_ptr<MockWorkingSetTrimmerPolicy> mock_policy(
      new MockWorkingSetTrimmerPolicy);
  auto process_node = CreateNode<ProcessNodeImpl>();
  ASSERT_NE(process_node.get(), nullptr);

  EXPECT_CALL(*mock_policy, TrimWorkingSet(process_node.get())).Times(1);
  graph()->PassToGraph(std::move(mock_policy));

  process_node->OnAllFramesInProcessFrozenForTesting();
}

}  // namespace policies
}  // namespace performance_manager
