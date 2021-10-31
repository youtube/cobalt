// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/submission_queue.h"
#include "base/time/time.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/renderer/submission.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::renderer::Submission;
using cobalt::renderer::SubmissionQueue;

namespace {
const int kMaxQueueSize = 4;

Submission MakeSubmissionWithUniqueRenderTree(
    const base::TimeDelta& time_offset) {
  // Create a dummy, but unique, render tree.
  cobalt::render_tree::CompositionNode::Builder builder;
  Submission submission(new cobalt::render_tree::CompositionNode(builder));
  submission.time_offset = time_offset;

  return submission;
}

Submission MakeSubmissionWithUniqueRenderTree(double time_offset_in_seconds) {
  return MakeSubmissionWithUniqueRenderTree(
      base::TimeDelta::FromSecondsD(time_offset_in_seconds));
}

base::TimeTicks SecondsToTime(double seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
}
}  // namespace

// Simple "make sure we get the single item out what we put in" test.
TEST(SubmissionQueueTest, SimpleSubmit) {
  SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1));

  // Make a submission with submission time of 1.0 seconds.
  Submission first = MakeSubmissionWithUniqueRenderTree(1.0);
  // Submit this to the queue at renderer time 2.0 seconds.
  queue.PushSubmission(first, SecondsToTime(2.0));

  // Get the current submission at the "same" time that the only queued
  // submission was pushed into it.
  Submission current = queue.GetCurrentSubmission(SecondsToTime(2.0));

  // Verify that it is the render tree we expect it to be and also that its
  // time is exactly equal to the submission's original time.
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(1.0, current.time_offset.InSecondsF());
}

// Test that the returned current submission has a time offset that increases
// as renderer time increases.
TEST(SubmissionQueueTest, SubmissionTimeOffsetIncreases) {
  SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1));

  Submission first = MakeSubmissionWithUniqueRenderTree(1.0);
  queue.PushSubmission(first, SecondsToTime(2.0));

  // Check that as we increase the renderer time, the current submission time
  // increases at the same pace.
  Submission current = queue.GetCurrentSubmission(SecondsToTime(2.0));
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(1.0, current.time_offset.InSecondsF());

  current = queue.GetCurrentSubmission(SecondsToTime(2.5));
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(1.5, current.time_offset.InSecondsF());

  current = queue.GetCurrentSubmission(SecondsToTime(6.0));
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(5.0, current.time_offset.InSecondsF());
}

// This test makes sure that if we queue multiple submissions, we will see each
// of them become the current submission as time moves forward.
TEST(SubmissionQueueTest, MultipleSubmissionsBecomeCurrentAsTimeAdvances) {
  // We choose a very long time for the convergence time so that the renderer
  // time will not skew much during this test.
  SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1000));

  Submission first = MakeSubmissionWithUniqueRenderTree(1.0);
  Submission second = MakeSubmissionWithUniqueRenderTree(2.0);
  Submission third = MakeSubmissionWithUniqueRenderTree(3.0);
  Submission fourth = MakeSubmissionWithUniqueRenderTree(4.0);

  queue.PushSubmission(first, SecondsToTime(2.0));
  queue.PushSubmission(second, SecondsToTime(2.1));
  queue.PushSubmission(third, SecondsToTime(2.2));
  queue.PushSubmission(fourth, SecondsToTime(2.3));

  // Check that as we increase the renderer time, the current submission time
  // increases at the same pace.
  Submission current = queue.GetCurrentSubmission(SecondsToTime(2.5));
  EXPECT_EQ(first.render_tree, current.render_tree);

  current = queue.GetCurrentSubmission(SecondsToTime(3.5));
  EXPECT_EQ(second.render_tree, current.render_tree);

  current = queue.GetCurrentSubmission(SecondsToTime(4.5));
  EXPECT_EQ(third.render_tree, current.render_tree);

  current = queue.GetCurrentSubmission(SecondsToTime(5.5));
  EXPECT_EQ(fourth.render_tree, current.render_tree);
}

TEST(SubmissionQueueTest, TimeSkewsTowardsFasterOffsets) {
  SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1));

  Submission first = MakeSubmissionWithUniqueRenderTree(1.0);
  Submission second = MakeSubmissionWithUniqueRenderTree(5.0);

  // Offset of 1.0 from render tree time.
  queue.PushSubmission(first, SecondsToTime(2.0));
  // Offset of 0.5 from render tree time.
  queue.PushSubmission(second, SecondsToTime(5.5));

  // Check that the first submission has up until this point had its time
  // advanced at the same rate as the renderer.
  Submission current = queue.GetCurrentSubmission(SecondsToTime(5.5));
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(4.5, current.time_offset.InSecondsF());

  // At this point we are half-way through the transition, where the renderer
  // moves from being offset 1.0 from the submission time to 0.5 from the
  // transition time.  At the half-way point, we should also be exactly half
  // way through the transition (assuming a cubic bezier is used), so that
  // means our offset should be 0.75 at this point, so we have moved 0.25
  // seconds forward in time.
  current = queue.GetCurrentSubmission(SecondsToTime(6.0));
  EXPECT_EQ(second.render_tree, current.render_tree);
  EXPECT_NEAR(5.25, current.time_offset.InSecondsF(), 0.001);

  // After 1 second later, we should always be at offset 0.5 from the submission
  // time.
  current = queue.GetCurrentSubmission(SecondsToTime(8.0));
  EXPECT_EQ(second.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(7.5, current.time_offset.InSecondsF());
}

TEST(SubmissionQueueTest,
     TimeDoesNotSkewTowardsFasterOffsetsWhenLatencyReductionIsDisabled) {
  SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1), false);

  Submission first = MakeSubmissionWithUniqueRenderTree(1.0);
  Submission second = MakeSubmissionWithUniqueRenderTree(5.0);

  // Offset of 1.0 from render tree time.
  queue.PushSubmission(first, SecondsToTime(2.0));
  // Offset of 0.5 from render tree time.
  queue.PushSubmission(second, SecondsToTime(5.5));

  // Check that the first submission has up until this point had its time
  // advanced at the same rate as the renderer.
  Submission current = queue.GetCurrentSubmission(SecondsToTime(5.5));
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(4.5, current.time_offset.InSecondsF());

  // At this point we are half-way through the transition.  Ordinarily the
  // submission queue would try to transition from the initial offset of 1.0
  // to the new offset of 0.5, but because latency reduction is disabled, the
  // offset should never increase and it should stay at 1.0.
  current = queue.GetCurrentSubmission(SecondsToTime(6.0));
  EXPECT_EQ(second.render_tree, current.render_tree);
  EXPECT_NEAR(5, current.time_offset.InSecondsF(), 0.001);

  // After 1 second later, we should still be unchanged from offset 1.0
  current = queue.GetCurrentSubmission(SecondsToTime(8.0));
  EXPECT_EQ(second.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(7, current.time_offset.InSecondsF());

  // A third submission that increases the latency should indeed still affect
  // the submission queue transitioned time.
  Submission third = MakeSubmissionWithUniqueRenderTree(9.5);
  queue.PushSubmission(third, SecondsToTime(11.0));

  // Nothing should have changed yet...
  current = queue.GetCurrentSubmission(SecondsToTime(11.0));
  EXPECT_EQ(third.render_tree, current.render_tree);
  EXPECT_NEAR(10, current.time_offset.InSecondsF(), 0.001);

  // We should be transitioning to a larger offset now...
  current = queue.GetCurrentSubmission(SecondsToTime(11.5));
  EXPECT_EQ(third.render_tree, current.render_tree);
  EXPECT_NEAR(10.25, current.time_offset.InSecondsF(), 0.001);
}

// Check that inserting a submission older than what the renderer thinks the
// submission time is will cause the SubmissionQueue to jump back in time to
// make to accommodate the newly pushed old submission.
TEST(SubmissionQueueTest, PushingOldTimeResetsQueue) {
  SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1));

  Submission first = MakeSubmissionWithUniqueRenderTree(1.0);
  Submission second = MakeSubmissionWithUniqueRenderTree(2.0);
  Submission third = MakeSubmissionWithUniqueRenderTree(3.0);

  queue.PushSubmission(first, SecondsToTime(2.0));
  queue.PushSubmission(second, SecondsToTime(2.5));

  // Check that we're still returning the first tree at this point.
  Submission current = queue.GetCurrentSubmission(SecondsToTime(2.5));
  EXPECT_EQ(first.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(1.5, current.time_offset.InSecondsF());

  queue.PushSubmission(third, SecondsToTime(5.0));

  // We expect the submission queue to not do anything rash at this point, even
  // though it was inserted late.
  current = queue.GetCurrentSubmission(SecondsToTime(5.0));
  EXPECT_EQ(third.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(4.5, current.time_offset.InSecondsF());

  // Make sure that we never go backwards in time, even if the difference
  // between the render time and submission time is shrinking quickly.
  current = queue.GetCurrentSubmission(SecondsToTime(5.5));
  EXPECT_EQ(third.render_tree, current.render_tree);
  EXPECT_DOUBLE_EQ(4.874633, current.time_offset.InSecondsF());
}

// This tests that pushing a submission into a full queue will result in the
// oldest submission to be forcefully removed, and time artificially advanced
// up to the next oldest submission.
TEST(SubmissionQueueTest, FullQueueWillEjectOldestSubmission) {
  // Loop over this with many different values to test that numerical precision
  // does not cause any problems here.
  for (int64 i = 0; i < 1000; ++i) {
    // We choose a very long time for the convergence time so that the renderer
    // time will not skew much during this test.
    SubmissionQueue queue(4, base::TimeDelta::FromSeconds(1000));

    base::TimeDelta start_time =
        base::TimeDelta::FromMicroseconds(i * i * i * i);

    Submission first = MakeSubmissionWithUniqueRenderTree(
        base::TimeDelta::FromSeconds(1) + start_time);
    Submission second = MakeSubmissionWithUniqueRenderTree(
        base::TimeDelta::FromSeconds(2) + start_time);
    Submission third = MakeSubmissionWithUniqueRenderTree(
        base::TimeDelta::FromSeconds(3) + start_time);
    Submission fourth = MakeSubmissionWithUniqueRenderTree(
        base::TimeDelta::FromSeconds(4) + start_time);

    queue.PushSubmission(first, SecondsToTime(2.0));
    queue.PushSubmission(second, SecondsToTime(2.1));
    queue.PushSubmission(third, SecondsToTime(2.2));
    queue.PushSubmission(fourth, SecondsToTime(2.3));

    Submission current = queue.GetCurrentSubmission(SecondsToTime(2.3));
    EXPECT_EQ(first.render_tree, current.render_tree);

    // Since the queue has a maximum size of 4 submissions, pushing this 5th
    // submission should result in the first submission being ejected, and time
    // being advanced to that of the second submission.
    Submission fifth = MakeSubmissionWithUniqueRenderTree(
        base::TimeDelta::FromSeconds(5) + start_time);
    queue.PushSubmission(fifth, SecondsToTime(2.4));

    current = queue.GetCurrentSubmission(SecondsToTime(2.4));
    EXPECT_EQ(second.render_tree, current.render_tree);
    EXPECT_EQ(base::TimeDelta::FromSeconds(2) + start_time,
              current.time_offset);
  }
}
