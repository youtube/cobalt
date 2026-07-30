// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/android/font_prewarmer_android.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontPrewarmerAndroidTest, PrewarmFamily) {
  base::test::TaskEnvironment task_environment;
  FontPrewarmer prewarmer;

  // Verify that calling PrewarmFamily does not crash.
  // The task will run on a background thread.
  prewarmer.PrewarmFamily("sans-serif");
  task_environment.RunUntilIdle();
}

}  // namespace blink
