// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_handler.h"

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(FeatureShowcaseHandlerTest, FinishFeatureShowcaseRunsCallback) {
  base::test::TestFuture<void> finish_future;

  FeatureShowcaseHandler handler(
      /*receiver=*/mojo::NullReceiver(), finish_future.GetCallback(),
      /*next_step_shown_callback=*/base::DoNothing());

  handler.FinishFeatureShowcase();
  EXPECT_TRUE(finish_future.Wait());
}

TEST(FeatureShowcaseHandlerTest, NextStepShownRunsCallback) {
  base::test::TestFuture<void> next_step_shown_future;

  FeatureShowcaseHandler handler(
      /*receiver=*/mojo::NullReceiver(),
      /*finish_callback=*/base::DoNothing(),
      next_step_shown_future.GetRepeatingCallback());
  handler.NextStepShown();

  EXPECT_TRUE(next_step_shown_future.Wait());
}

}  // namespace
