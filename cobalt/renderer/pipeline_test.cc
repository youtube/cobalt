// Copyright 2014 Google Inc. All Rights Reserved.
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

#include <cmath>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "base/threading/platform_thread.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/submission.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Between;
using ::testing::_;
using cobalt::renderer::Pipeline;
using cobalt::renderer::rasterizer::Rasterizer;

namespace {
// We explicitly state here how long our mock rasterizer will take to complete
// a submission.  This value is chosen to reflect the refresh rate of a typical
// display device.
const base::TimeDelta kRasterizeDelay =
    base::TimeDelta::FromSecondsD(1.0 / 60.0);

// Unfortunately, we can't make use of gmock to test the number of submission
// calls that were made.  This is because we need to test our expectations
// AFTER all Submit() calls have been made, whereas gmock requires us to
// set these expect calls first.  The reason we must set them after is because
// the expected number of calls made to Submit() is dependent on the amount of
// time that has passed, and so we need to wait until all calls have completed
// before we know how long it had taken.
class MockRasterizer : public Rasterizer {
 public:
  explicit MockRasterizer(int* submission_count)
      : submission_count_(submission_count) {}

  void Submit(const scoped_refptr<cobalt::render_tree::Node>&,
              const scoped_refptr<cobalt::renderer::backend::RenderTarget>&,
              const Options& options) override {
    if (last_submission_time) {
      // Simulate a "wait for vsync".
      base::TimeDelta since_last_submit =
          base::TimeTicks::Now() - *last_submission_time;

      base::TimeDelta sleep_time = kRasterizeDelay - since_last_submit;
      if (sleep_time > base::TimeDelta()) {
        base::PlatformThread::Sleep(kRasterizeDelay - since_last_submit);
      }
    }
    last_submission_time = base::TimeTicks::Now();

    ++(*submission_count_);
  }

  cobalt::render_tree::ResourceProvider* GetResourceProvider() override {
    return NULL;
  }

  void MakeCurrent() override {}

 private:
  int* submission_count_;
  base::optional<base::TimeTicks> last_submission_time;
};

scoped_ptr<Rasterizer> CreateMockRasterizer(int* submission_count) {
  return scoped_ptr<Rasterizer>(new MockRasterizer(submission_count));
}
}  // namespace

class RendererPipelineTest : public ::testing::Test {
 protected:
  RendererPipelineTest() {
    submission_count_ = 0;
    start_time_ = base::TimeTicks::Now();
    pipeline_.reset(
        new Pipeline(base::Bind(&CreateMockRasterizer, &submission_count_),
                     NULL, NULL, true, Pipeline::kNoClear));
  }

  static void DummyAnimateFunction(
      cobalt::render_tree::CompositionNode::Builder* composition_node_builder,
      base::TimeDelta time) {}

  // We create a render tree composed of only a single, empty CompositionNode
  // that is meant to act as a dummy/placeholder.  It is animated to ensure that
  // it changes every frame and so the Pipeline must rasterize it each frame.
  scoped_refptr<cobalt::render_tree::Node> MakeDummyAnimatedRenderTree() {
    cobalt::render_tree::CompositionNode::Builder builder;
    scoped_refptr<cobalt::render_tree::CompositionNode> dummy_node(
        new cobalt::render_tree::CompositionNode(builder));

    cobalt::render_tree::animations::AnimateNode::Builder animate_builder;
    animate_builder.Add(dummy_node, base::Bind(&DummyAnimateFunction));
    scoped_refptr<cobalt::render_tree::animations::AnimateNode> animate_node(
        new cobalt::render_tree::animations::AnimateNode(animate_builder,
                                                         dummy_node));

    return animate_node;
  }
  // Checks that Submit() was called on mock_rasterizer_ the expect number of
  // times given the refresh rate and lower/upper bounds for how long the
  // pipeline was active.
  struct ExpectedNumberOfSubmissions {
    int lower_bound;
    int upper_bound;
  };

  // Given the lower/upper bounds on the length of time the pipeline's been
  // active for, return a lower and upper bound on the expected number of
  // submissions that should have occurred.
  ExpectedNumberOfSubmissions GetExpectedNumberOfSubmissions(
      base::TimeDelta duration_lower_bound,
      base::TimeDelta duration_upper_bound) {
    // We should have submitted the render tree to the rasterizer at the
    // specified frequency during the duration that we waited for.
    // Since we're not always running on a real-time operating system, the
    // number of Submit()s called has the potential to be flaky.  We account for
    // this by allowing for some error.
    const int kFuzzRangeOnExpectedSubmissions = 1;
    ExpectedNumberOfSubmissions number_of_submissions;
    number_of_submissions.lower_bound =
        static_cast<int>(floor(duration_lower_bound.InSecondsF() /
                               kRasterizeDelay.InSecondsF())) -
        kFuzzRangeOnExpectedSubmissions;
    number_of_submissions.upper_bound =
        static_cast<int>(ceil(duration_upper_bound.InSecondsF() /
                              kRasterizeDelay.InSecondsF())) +
        kFuzzRangeOnExpectedSubmissions;
    return number_of_submissions;
  }

  base::TimeTicks start_time_;  // Record the time that we started the pipeline.
  scoped_ptr<Pipeline> pipeline_;
  int submission_count_;
};

// Since the following tests test the pipeline's claim that it will rasterize
// the submitted render tree at a specified real time refresh rate, they are
// marked as flaky, as it is possible for the OS to schedule (or rather, not
// schedule) the pipeline rasterizer thread in such a way that it is not able
// to execute frequently enough.  While this may be a problem on machines like
// Windows and Linux which may have other processes running in the background,
// devices like game consoles where our process is running alone should never
// see these tests fail.
TEST_F(
    RendererPipelineTest,
    FLAKY_RasterizerSubmitCalledAtExpectedFrequencyAfterSinglePipelineSubmit) {
  pipeline_->Submit(
      cobalt::renderer::Submission(MakeDummyAnimatedRenderTree()));

  // Wait a little bit to give the pipeline some time to rasterize the submitted
  // render tree.
  const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(200);
  base::PlatformThread::Sleep(kDelay);

  // Shut down the pipeline so that Submit will no longer be called.
  pipeline_.reset();

  // Find the time that the pipeline's been active for.
  base::TimeDelta time_elapsed = base::TimeTicks::Now() - start_time_;

  ExpectedNumberOfSubmissions expected_submissions =
      GetExpectedNumberOfSubmissions(kDelay, time_elapsed);
  EXPECT_LE(expected_submissions.lower_bound, submission_count_);
  EXPECT_GE(expected_submissions.upper_bound, submission_count_);
}

TEST_F(
    RendererPipelineTest,
    FLAKY_RasterizerSubmitCalledAtExpectedFrequencyAfterManyPipelineSubmits) {
  // Here we repeatedly submit a new render tree to the pipeline as fast as
  // we can.  Regardless of the rate that we submit to the pipeline, we expect
  // it to rate-limit its submissions to the rasterizer.
  const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(200);
  while (true) {
    base::TimeDelta time_elapsed = base::TimeTicks::Now() - start_time_;
    // Stop after kDelay seconds have passed.
    if (time_elapsed > kDelay) {
      break;
    }

    pipeline_->Submit(
        cobalt::renderer::Submission(MakeDummyAnimatedRenderTree()));

    const base::TimeDelta kSubmitDelay(base::TimeDelta::FromMilliseconds(1));
    // While we want to submit faster than the rasterizer is rasterizing,
    // we still should pace ourselves so we that we ensure the rasterizer has
    // enough time to process all the submissions.
    ASSERT_LT(kSubmitDelay, kRasterizeDelay);
    base::PlatformThread::Sleep(kSubmitDelay);
  }

  // Shut down the pipeline so that Submit will no longer be called.
  pipeline_.reset();

  // Find the time that the pipeline's been active for.
  base::TimeDelta time_elapsed = base::TimeTicks::Now() - start_time_;

  ExpectedNumberOfSubmissions expected_submissions =
      GetExpectedNumberOfSubmissions(kDelay, time_elapsed);
  EXPECT_LE(expected_submissions.lower_bound, submission_count_);
  EXPECT_GE(expected_submissions.upper_bound, submission_count_);
}
