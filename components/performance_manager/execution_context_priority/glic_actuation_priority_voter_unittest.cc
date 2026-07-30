// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/glic_actuation_priority_voter.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

class GlicActuationPriorityVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  GlicActuationPriorityVoterTest() = default;
  ~GlicActuationPriorityVoterTest() override = default;

  GlicActuationPriorityVoterTest(const GlicActuationPriorityVoterTest&) =
      delete;
  GlicActuationPriorityVoterTest& operator=(
      const GlicActuationPriorityVoterTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    page_live_state_decorator_ = std::make_unique<PageLiveStateDecorator>();
    graph()->PassToGraph(std::move(page_live_state_decorator_));
    glic_voter_.InitializeOnGraph(graph(), observer_.BuildVotingChannel());
  }

  void TearDown() override {
    glic_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  VoterId voter_id() const { return glic_voter_.voter_id(); }

  DummyVoteObserver observer_;
  std::unique_ptr<PageLiveStateDecorator> page_live_state_decorator_;
  GlicActuationPriorityVoter glic_voter_;
};

}  // namespace

// Tests that a USER_BLOCKING vote is cast ONLY for the main frame (and not a
// child frame) when a page is Glic actuating.
TEST_F(GlicActuationPriorityVoterTest, VoteWhenActuatingWithChildFrame) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();
  auto* child_frame_node = mock_graph.child_frame.get();

  // No votes initially.
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Set to Glic actuating on visible tab, expect a USER_BLOCKING vote ONLY on
  // the main frame.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->SetGlicActuationStateForTesting(
          GlicActuationState::kActuatingOnVisibleTab);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node),
                        base::Process::Priority::kUserBlocking,
                        GlicActuationPriorityVoter::kGlicActuationReason));
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(child_frame_node)));

  // Change to actuating on background tab, expect a USER_VISIBLE vote.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->SetGlicActuationStateForTesting(
          GlicActuationState::kActuatingOnBackgroundTab);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node),
                        base::Process::Priority::kUserVisible,
                        GlicActuationPriorityVoter::kGlicActuationReason));

  // Set back to none, expect the vote to be invalidated.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->SetGlicActuationStateForTesting(GlicActuationState::kNone);
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
  EXPECT_FALSE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));
}

// Tests that a child frame added to an actuating page does NOT get a vote.
TEST_F(GlicActuationPriorityVoterTest,
       ChildFrameAddedToActuatingPageGetsNoVote) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  auto* main_frame_node = mock_graph.frame.get();

  // Set to actuating, expect a USER_BLOCKING vote on the main frame.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->SetGlicActuationStateForTesting(
          GlicActuationState::kActuatingOnVisibleTab);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(
      observer_.HasVote(voter_id(), GetExecutionContext(main_frame_node)));

  // Add a child frame, expect NO additional vote.
  auto child_frame_node = graph()->CreateFrameNodeAutoId(
      mock_graph.process.get(), page_node, main_frame_node);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_FALSE(observer_.HasVote(voter_id(),
                                 GetExecutionContext(child_frame_node.get())));

  // Set back to not actuating, expect all votes to be invalidated.
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
      ->SetGlicActuationStateForTesting(GlicActuationState::kNone);
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

}  // namespace performance_manager::execution_context_priority
