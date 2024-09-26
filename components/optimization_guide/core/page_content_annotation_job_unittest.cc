// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_content_annotation_job.h"

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PageContentAnnotationJobTest : public testing::Test {
 public:
  PageContentAnnotationJobTest() = default;
  ~PageContentAnnotationJobTest() override = default;

  void OnBatchAnnotationComplete(
      std::vector<BatchAnnotationResult>* out,
      const std::vector<BatchAnnotationResult>& results) {
    *out = results;
  }
};

TEST_F(PageContentAnnotationJobTest, IteratesInput) {
  PageContentAnnotationJob job(base::NullCallback(), {"1", "2", "3"},
                               AnnotationType::kPageTopics);
  absl::optional<std::string> input;

  input = job.GetNextInput();
  ASSERT_TRUE(input);
  EXPECT_EQ("1", *input);

  input = job.GetNextInput();
  ASSERT_TRUE(input);
  EXPECT_EQ("2", *input);

  input = job.GetNextInput();
  ASSERT_TRUE(input);
  EXPECT_EQ("3", *input);

  EXPECT_FALSE(job.GetNextInput());
}

TEST_F(PageContentAnnotationJobTest, Callback) {
  std::vector<BatchAnnotationResult> results;
  PageContentAnnotationJob job(
      base::BindOnce(&PageContentAnnotationJobTest::OnBatchAnnotationComplete,
                     base::Unretained(this), &results),
      {"1", "2", "3"}, AnnotationType::kPageTopics);

  // Drain the inputs before running the callback.
  while (job.GetNextInput()) {
  }

  BatchAnnotationResult expected =
      BatchAnnotationResult::CreatePageTopicsResult("1", absl::nullopt);

  job.PostNewResult(expected, 0);
  job.OnComplete();

  ASSERT_EQ(3U, results.size());
  EXPECT_EQ(expected, results[0]);
  EXPECT_EQ(BatchAnnotationResult::CreateEmptyAnnotationsResult(std::string()),
            results[1]);
  EXPECT_EQ(BatchAnnotationResult::CreateEmptyAnnotationsResult(std::string()),
            results[2]);
}

TEST_F(PageContentAnnotationJobTest, DeathOnUncompleted) {
  PageContentAnnotationJob job(base::NullCallback(), {"1", "2", "3"},
                               AnnotationType::kPageTopics);
  EXPECT_TRUE(job.GetNextInput());
  EXPECT_DCHECK_DEATH(job.OnComplete());
}

}  // namespace optimization_guide