// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/glass_frame_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

class GlassFrameServiceInteractiveTest : public InProcessBrowserTest {
 public:
  GlassFrameServiceInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlassFrame);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, GetInstance) {
  EXPECT_TRUE(GlassFrameService::GetInstance());
}
