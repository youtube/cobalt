// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

class SyntheticInputTest : public ContentBrowserTest {
 public:
  SyntheticInputTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableGpuBenchmarking);
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetRenderViewHost()
                                          ->GetWidget());
  }

  void LoadURL(const char* url) {
    const GURL data_url(url);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetRenderWidgetHost();
    HitTestRegionObserver observer(GetRenderWidgetHost()->GetFrameSinkId());
    host->GetView()->SetSize(gfx::Size(400, 400));

    std::u16string ready_title(u"ready");
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    std::ignore = watcher.WaitAndGetTitle();

    // Wait for the hit test data to be ready after initiating URL loading
    // before returning
    observer.WaitForHitTestData();
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

 protected:
  std::unique_ptr<base::RunLoop> runner_;
};

class GestureScrollObserver : public RenderWidgetHost::InputEventObserver {
 public:
  void OnInputEvent(const RenderWidgetHost& widget,
                    const blink::WebInputEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin)
      gesture_scroll_seen_ = true;
  }
  bool HasSeenGestureScrollBegin() const { return gesture_scroll_seen_; }
  bool gesture_scroll_seen_ = false;
};

// This test checks that we destroying a render widget host with an ongoing
// gesture doesn't cause lifetime issues. Namely, that the gesture
// CompletionCallback isn't destroyed before being called or the Mojo pipe
// being closed.
IN_PROC_BROWSER_TEST_F(SyntheticInputTest, DestroyWidgetWithOngoingGesture) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  GestureScrollObserver gesture_observer;

  GetRenderWidgetHost()->AddInputEventObserver(&gesture_observer);

  // By starting a gesture, there's a Mojo callback that the renderer is
  // waiting on the browser to resolve. If the browser is shutdown before
  // ACKing the callback or closing the channel, we'll DCHECK.
  ASSERT_TRUE(
      ExecJs(shell()->web_contents(),
             "chrome.gpuBenchmarking.smoothScrollByXY(0, 10000, ()=>{}, "
             "100, 100, chrome.gpuBenchmarking.TOUCH_INPUT);"));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return gesture_observer.HasSeenGestureScrollBegin(); }));

  shell()->Close();
}

// This test ensures that synthetic wheel scrolling works on all platforms.
// Disabled for flakiness on Android (crbug.com/1103731).
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SmoothScrollWheel DISABLED_SmoothScrollWheel
#else
#define MAYBE_SmoothScrollWheel SmoothScrollWheel
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SyntheticInputTest, MAYBE_SmoothScrollWheel) {
  LoadURL(R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <meta name='viewport' content='width=device-width'>
    <style>
      body {
        width: 10px;
        height: 2000px;
      }
    </style>
    <script>
      document.title = 'ready';
    </script>
  )HTML");

  // Note: 256 is precisely chosen since Android's minimum granularity is 64px.
  // All other platforms can specify the delta per-pixel.
  const int scroll_delta = 256;

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kMouseInput;
  params.anchor = gfx::PointF(1, 1);
  params.distances.push_back(gfx::Vector2d(0, -scroll_delta));

  // Use a speed that's fast enough that the entire scroll occurs in a single
  // GSU, avoiding precision loss. SyntheticGestures can lose delta over time
  // in slower scrolls on some platforms.
  params.speed_in_pixels_s = 10000000.f;

  // Use PrecisePixel to avoid animating.
  params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;

  runner_ = std::make_unique<base::RunLoop>();

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  GetRenderWidgetHost()->QueueSyntheticGesture(
      std::move(gesture),
      base::BindOnce(&SyntheticInputTest::OnSyntheticGestureCompleted,
                     base::Unretained(this)));

  // Run until we get the OnSyntheticGestureCompleted callback
  runner_->Run();
  runner_.reset();

  EXPECT_EQ(scroll_delta, EvalJs(shell()->web_contents(),
                                 "document.scrollingElement.scrollTop"));
}

// This test ensures that slow synthetic wheel scrolling does not lose precision
// over time.
// https://crbug.com/1103731. Flaky on Android bots.
// https://crbug.com/1086334. Flaky on all desktop bots, but maybe for a
// different reason.
IN_PROC_BROWSER_TEST_F(SyntheticInputTest, DISABLED_SlowSmoothScrollWheel) {
  LoadURL(R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <meta name='viewport' content='width=device-width'>
    <style>
      body {
        width: 10px;
        height: 2000px;
      }
    </style>
    <script>
      document.title = 'ready';
    </script>
  )HTML");

  // Note: 1024 is precisely chosen since Android's minimum granularity is 64px.
  // All other platforms can specify the delta per-pixel.
  const int scroll_delta = 1024;

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kMouseInput;
  params.anchor = gfx::PointF(1, 1);
  params.distances.push_back(gfx::Vector2d(0, -scroll_delta));

  // Use a speed that's slow enough that it requires the browser to require
  // multiple wheel-events to be dispatched, so that precision is needed to
  // scroll the correct amount.
  params.speed_in_pixels_s = 1000.f;

  // Use PrecisePixel to avoid animating.
  params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;

  runner_ = std::make_unique<base::RunLoop>();

  auto* web_contents = shell()->web_contents();
  RenderFrameSubmissionObserver scroll_offset_wait(web_contents);
  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  GetRenderWidgetHost()->QueueSyntheticGesture(
      std::move(gesture),
      base::BindOnce(&SyntheticInputTest::OnSyntheticGestureCompleted,
                     base::Unretained(this)));
  float device_scale_factor =
      web_contents->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  scroll_offset_wait.WaitForScrollOffset(
      gfx::PointF(0.f, ((float)scroll_delta) * device_scale_factor));

  EXPECT_EQ(scroll_delta, EvalJs(shell()->web_contents(),
                                 "document.scrollingElement.scrollTop"));
}

}  // namespace content
