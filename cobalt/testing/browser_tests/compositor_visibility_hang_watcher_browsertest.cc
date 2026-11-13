// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/threading_features.h"
#include "base/time/time.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

namespace cobalt {

class CompositorVisibilityHangWatcherBrowserTest
    : public content::ContentBrowserTest {
 public:
  CompositorVisibilityHangWatcherBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        base::kEnableHangWatcher, {{"threadpool_log_level", "0"}});
  }
  ~CompositorVisibilityHangWatcherBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    base::HangWatcher::InitializeOnMainThread(
        base::HangWatcher::ProcessType::kBrowserProcess,
        /*emit_crashes=*/false);
    hang_watcher_ = std::make_unique<base::HangWatcher>();
    hang_watcher_->SetMonitoringPeriodForTesting(base::Hours(10));
    hang_watcher_->Start();
  }

  void TearDownOnMainThread() override {
    hang_watcher_.reset();
    base::HangWatcher::UninitializeOnMainThreadForTesting();
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::HangWatcher> hang_watcher_;
};

IN_PROC_BROWSER_TEST_F(CompositorVisibilityHangWatcherBrowserTest,
                       SuspendsWhenHiddenAndResumesWhenVisible) {
  ASSERT_TRUE(
      content::NavigateToURL(shell()->web_contents(), GURL("about:blank")));
  ASSERT_TRUE(base::HangWatcher::IsEnabled());

  ui::WindowAndroid* window =
      shell()->web_contents()->GetTopLevelNativeWindow();
  ASSERT_NE(window, nullptr);
  auto* compositor =
      static_cast<content::CompositorImpl*>(window->GetCompositor());
  ASSERT_NE(compositor, nullptr);

  compositor->SetVisibleForTesting(false);
  EXPECT_FALSE(base::HangWatcher::IsEnabled());

  compositor->SetVisibleForTesting(true);
  EXPECT_TRUE(base::HangWatcher::IsEnabled());
}

}  // namespace cobalt
