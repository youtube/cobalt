/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cmath>

#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/renderer/rasterizer.h"
#include "cobalt/renderer/pipeline.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Between;
using cobalt::renderer::Rasterizer;
using cobalt::renderer::Pipeline;

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

  void Submit(
      const scoped_refptr<cobalt::render_tree::Node>&,
      const scoped_refptr<cobalt::renderer::backend::RenderTarget>&) OVERRIDE {
    ++(*submission_count_);
  }

  cobalt::render_tree::ResourceProvider* GetResourceProvider() OVERRIDE {
    return NULL;
  }

 private:
  int* submission_count_;
};

namespace {
scoped_ptr<Rasterizer> CreateMockRasterizer(int* submission_count) {
  return scoped_ptr<Rasterizer>(new MockRasterizer(submission_count));
}
}  // namespace

class RendererPipelineTest : public ::testing::Test {
 protected:
  RendererPipelineTest() {
    submission_count_ = 0;
    start_time_ = base::Time::Now();
    pipeline_.reset(new Pipeline(
        base::Bind(&CreateMockRasterizer, &submission_count_), NULL));
    refresh_rate_ = pipeline_->refresh_rate();

    // We create a render tree here composed of only a single, empty
    // CompositionNode that is meant to act as a dummy/placeholder.
    dummy_render_tree_ = scoped_refptr<cobalt::render_tree::Node>(
        new cobalt::render_tree::CompositionNode(
            cobalt::render_tree::CompositionNode::Builder()));
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
        static_cast<int>(
            floor(duration_lower_bound.InSecondsF() * refresh_rate_)) -
        kFuzzRangeOnExpectedSubmissions;
    number_of_submissions.upper_bound =
        static_cast<int>(
            ceil(duration_upper_bound.InSecondsF() * refresh_rate_)) +
        kFuzzRangeOnExpectedSubmissions;
    return number_of_submissions;
  }

  base::Time start_time_;  // Record the time that we started the pipeline.
  scoped_ptr<Pipeline> pipeline_;
  scoped_refptr<cobalt::render_tree::Node> dummy_render_tree_;
  float refresh_rate_;
  int submission_count_;
};

TEST_F(RendererPipelineTest,
       RasterizerSubmitIsCalledAtExpectedFrequencyAfterSinglePipelineSubmit) {
  pipeline_->Submit(dummy_render_tree_);

  // Wait a little bit to give the pipeline some time to rasterize the submitted
  // render tree.
  const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(200);
  base::PlatformThread::Sleep(kDelay);

  // Shut down the pipeline so that Submit will no longer be called.
  pipeline_.reset();

  // Find the time that the pipeline's been active for.
  base::TimeDelta time_elapsed = base::Time::Now() - start_time_;

  ExpectedNumberOfSubmissions expected_submissions =
      GetExpectedNumberOfSubmissions(kDelay, time_elapsed);
  EXPECT_LE(expected_submissions.lower_bound, submission_count_);
  EXPECT_GE(expected_submissions.upper_bound, submission_count_);
}

TEST_F(RendererPipelineTest,
       RasterizerSubmitIsCalledAtExpectedFrequencyAfterManyPipelineSubmits) {
  // Here we repeatedly submit a new render tree to the pipeline as fast as
  // we can.  Regardless of the rate that we submit to the pipeline, we expect
  // it to rate-limit its submissions to the rasterizer.
  const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(200);
  while (true) {
    base::TimeDelta time_elapsed = base::Time::Now() - start_time_;
    // Stop after kDelay seconds have passed.
    if (time_elapsed > kDelay) {
      break;
    }

    pipeline_->Submit(dummy_render_tree_);
  }

  // Shut down the pipeline so that Submit will no longer be called.
  pipeline_.reset();

  // Find the time that the pipeline's been active for.
  base::TimeDelta time_elapsed = base::Time::Now() - start_time_;

  ExpectedNumberOfSubmissions expected_submissions =
      GetExpectedNumberOfSubmissions(kDelay, time_elapsed);
  EXPECT_LE(expected_submissions.lower_bound, submission_count_);
  EXPECT_GE(expected_submissions.upper_bound, submission_count_);
}
