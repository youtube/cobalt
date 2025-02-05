// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/renderer/cobalt_render_frame_observer.h"

#include "cobalt/configuration/configuration.h"
#include "content/public/test/render_view_test.h"

namespace cobalt {

class CobaltRenderFrameObserverTest : public content::RenderViewTest {
 public:
  void SetUp() override {
    content::RenderViewTest::SetUp();
    observer = new CobaltRenderFrameObserver(GetMainRenderFrame());
  }

  void set_user_on_exit_strategy_callback(base::RepeatingClosure closure) {
    observer->set_user_on_exit_strategy_callback_for_testing(closure);
  }

 private:
  CobaltRenderFrameObserver* observer;
};

TEST_F(CobaltRenderFrameObserverTest, CloseOverrideTest) {
  int call_count = 0;
  set_user_on_exit_strategy_callback(base::BindRepeating(
      [](int* call_count) { (*call_count)++; }, &call_count));
  EXPECT_EQ(0, call_count);
  ExecuteJavaScriptForTests("window.close();");
  EXPECT_EQ(1, call_count);
}

TEST_F(CobaltRenderFrameObserverTest, MinimizeOverrideTest) {
  int call_count = 0;
  set_user_on_exit_strategy_callback(base::BindRepeating(
      [](int* call_count) { (*call_count)++; }, &call_count));
  EXPECT_EQ(0, call_count);
  ExecuteJavaScriptForTests("window.minimize();");
  EXPECT_EQ(1, call_count);
}

TEST_F(CobaltRenderFrameObserverTest, UserOnExitStrategyTest) {
  int user_on_exit_strategy = -1;
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(
      u"window.h5vcc.system.userOnExitStrategy", &user_on_exit_strategy));
  EXPECT_GE(user_on_exit_strategy, 0);
  EXPECT_LT(user_on_exit_strategy,
            static_cast<int32_t>(
                configuration::Configuration::UserOnExitStrategy::kCount));
}

}  // namespace cobalt
