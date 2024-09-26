// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/oom_score_policy_lacros.h"

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"  // For GraphTestHarnessWithMockDiscarder
#include "components/performance_manager/test_support/mock_graphs.h"  // For TestProcessNodeImpl
#include "content/public/common/content_constants.h"  // For kLowestRendererOomScore
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace policies {

class MockOomScorePolicyLacros : public OomScorePolicyLacros {
 public:
  // Overrides OnPassedToGraph to avoid starting the timer.
  void OnPassedToGraph(Graph* graph) override { graph_ = graph; }

  void AssignOomScores() { OomScorePolicyLacros::AssignOomScores(); }

  int GetCachedOomScore(base::ProcessId pid) {
    return OomScorePolicyLacros::GetCachedOomScore(pid);
  }
};

class OomScorePolicyLacrosTest
    : public testing::GraphTestHarnessWithMockDiscarder {
 public:
  OomScorePolicyLacrosTest() = default;
  ~OomScorePolicyLacrosTest() override = default;
  OomScorePolicyLacrosTest(const OomScorePolicyLacrosTest& other) = delete;
  OomScorePolicyLacrosTest& operator=(const OomScorePolicyLacrosTest&) = delete;

  void SetUp() override {
    testing::GraphTestHarnessWithMockDiscarder::SetUp();

    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<MockOomScorePolicyLacros>();
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    graph()->TakeFromGraph(policy_);
    testing::GraphTestHarnessWithMockDiscarder::TearDown();
  }

  MockOomScorePolicyLacros* policy() { return policy_; }

 private:
  raw_ptr<MockOomScorePolicyLacros> policy_;
};

TEST_F(OomScorePolicyLacrosTest, DistributeOomScores) {
  constexpr base::ProcessId kProcessId1 = 1;
  constexpr base::ProcessId kProcessId2 = 2;
  constexpr base::ProcessId kProcessId3 = 3;

  // Creates 3 pages.
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  main_frame_node1->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node1->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(kProcessId2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node2->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node3 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node3->SetProcessWithPid(kProcessId3, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  main_frame_node3->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node3->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  policy()->AssignOomScores();

  const int kMiddleRendererOomScore =
      (content::kHighestRendererOomScore + content::kLowestRendererOomScore) /
      2;
  ASSERT_EQ(policy()->GetCachedOomScore(process_node1->process_id()),
            content::kHighestRendererOomScore);
  ASSERT_EQ(policy()->GetCachedOomScore(process_node2->process_id()),
            kMiddleRendererOomScore);
  ASSERT_EQ(policy()->GetCachedOomScore(process_node3->process_id()),
            content::kLowestRendererOomScore);
  // GetCachedOomScore should return -1 for non-cached pid.
  ASSERT_EQ(policy()->GetCachedOomScore(0), -1);
}

TEST_F(OomScorePolicyLacrosTest, DistributeOomScoresWithPriority) {
  constexpr base::ProcessId kProcessId1 = 1;
  constexpr base::ProcessId kProcessId2 = 2;
  constexpr base::ProcessId kProcessId3 = 3;

  // Creates 3 pages.
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  main_frame_node1->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node1->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));
  // Set page node 1 audible to raise its priority.
  page_node1->SetIsAudible(true);

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(kProcessId2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node2->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node3 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node3->SetProcessWithPid(kProcessId3, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  main_frame_node3->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node3->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  policy()->AssignOomScores();

  const int kMiddleRendererOomScore =
      (content::kHighestRendererOomScore + content::kLowestRendererOomScore) /
      2;

  // Because page node 1 is audible, the corresponding process should have
  // lowest oom score adj.
  ASSERT_EQ(policy()->GetCachedOomScore(process_node2->process_id()),
            content::kHighestRendererOomScore);
  ASSERT_EQ(policy()->GetCachedOomScore(process_node3->process_id()),
            kMiddleRendererOomScore);
  ASSERT_EQ(policy()->GetCachedOomScore(process_node1->process_id()),
            content::kLowestRendererOomScore);
}

TEST_F(OomScorePolicyLacrosTest, DistributeOomScoresSharedPid) {
  constexpr base::ProcessId kProcessId1 = 1;
  constexpr base::ProcessId kProcessId2 = 2;
  constexpr base::ProcessId kProcessId3 = 3;

  // Creates 4 pages. 2 of the 4pages share the same renderer process.
  //
  // From earliest visible to latest visible pages:
  //   page_node1, page_node2, page_node3, page_node4
  //
  // Page to process relation:
  //   page_node1 -> process_node1
  //   page_node2 -> process_node2
  //   page_node3 -> process_node3
  //   page_node4 -> process_node2
  //
  // process_node2 is used by both page_node2 and page_node4, the latest visible
  // page of the 2 pages would be used to estimate process priority.
  //
  // From most important process to least important process:
  // process_node2, process_node3, process_node1
  auto process_node1 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node1->SetProcessWithPid(kProcessId1, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());
  auto page_node1 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node1 =
      CreateFrameNodeAutoId(process_node1.get(), page_node1.get());
  main_frame_node1->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node1.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node1->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node2 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node2->SetProcessWithPid(kProcessId2, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());

  // page_node2 and page_node4 share the same renderer process.
  auto page_node2 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node2 =
      CreateFrameNodeAutoId(process_node2.get(), page_node2.get());
  main_frame_node2->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node2.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node2->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto process_node3 = TestNodeWrapper<TestProcessNodeImpl>::Create(graph());
  process_node3->SetProcessWithPid(kProcessId3, base::Process::Current(),
                                   /* launch_time=*/base::TimeTicks::Now());

  auto page_node3 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node3 =
      CreateFrameNodeAutoId(process_node3.get(), page_node3.get());
  main_frame_node3->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node3.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node3->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  auto page_node4 = CreateNode<performance_manager::PageNodeImpl>();
  auto main_frame_node4 =
      CreateFrameNodeAutoId(process_node2.get(), page_node4.get());
  main_frame_node4->SetIsCurrent(true);
  testing::MakePageNodeDiscardable(page_node4.get(), task_env());
  AdvanceClock(base::Minutes(30));
  main_frame_node4->SetIsCurrent(false);
  AdvanceClock(base::Minutes(30));

  policy()->AssignOomScores();

  const int kMiddleRendererOomScore =
      (content::kHighestRendererOomScore + content::kLowestRendererOomScore) /
      2;
  ASSERT_EQ(policy()->GetCachedOomScore(process_node1->process_id()),
            content::kHighestRendererOomScore);
  ASSERT_EQ(policy()->GetCachedOomScore(process_node3->process_id()),
            kMiddleRendererOomScore);
  ASSERT_EQ(policy()->GetCachedOomScore(process_node2->process_id()),
            content::kLowestRendererOomScore);
}
}  // namespace policies
}  // namespace performance_manager
