// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/test_event_rewriter.h"
#include "ui/platform_window/stub/stub_window.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/native_window_occlusion_tracker_win.h"
#endif

namespace aura {

namespace {

// A convenient wrapper that makes it easy to invoke this method inside an
// EXPECT_EQ statement.
gfx::Point ConvertDIPToPixels(const WindowTreeHost* host, gfx::Point point) {
  host->ConvertDIPToPixels(&point);
  return point;
}

// A convenient wrapper that makes it easy to invoke this method inside an
// EXPECT_EQ statement.
gfx::PointF ConvertDIPToPixels(const WindowTreeHost* host, gfx::PointF point) {
  host->ConvertDIPToPixels(&point);
  return point;
}

// A convenient wrapper that makes it easy to invoke this method inside an
// EXPECT_EQ statement.
gfx::Point ConvertPixelsToDIP(const WindowTreeHost* host, gfx::Point point) {
  host->ConvertPixelsToDIP(&point);
  return point;
}

// A convenient wrapper that makes it easy to invoke this method inside an
// EXPECT_EQ statement.
gfx::PointF ConvertPixelsToDIP(const WindowTreeHost* host, gfx::PointF point) {
  host->ConvertPixelsToDIP(&point);
  return point;
}

}  // namespace

using WindowTreeHostTest = test::AuraTestBase;

TEST_F(WindowTreeHostTest, DPIWindowSize) {
  constexpr gfx::Rect starting_bounds(
      aura::test::AuraTestHelper::kDefaultHostSize);

  EXPECT_EQ(starting_bounds.size(), host()->compositor()->size());
  EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
  EXPECT_EQ(starting_bounds, root_window()->bounds());

  test_screen()->SetDeviceScaleFactor(1.5f);
  EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
  // Size should be rounded up after scaling.
  EXPECT_EQ(gfx::Rect(0, 0, 534, 400), root_window()->bounds());

  gfx::Transform transform;
  transform.Translate(0, -1.1f);
  host()->SetRootTransform(transform);
  EXPECT_EQ(gfx::Rect(0, 1, 534, 401), root_window()->bounds());

  EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
  EXPECT_EQ(gfx::Rect(0, 1, 534, 401), root_window()->bounds());
}

TEST_F(WindowTreeHostTest,
       ShouldHaveExactRootWindowBoundsWithDisplayRotation1xScale) {
  test_screen()->SetDeviceScaleFactor(1.f);

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_0);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_0);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(gfx::Rect(400, 300), host()->window()->bounds());

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_90);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(300, 400));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 300, 400));
  EXPECT_EQ(gfx::Rect(300, 400), host()->window()->bounds());

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_180);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_180);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(gfx::Rect(400, 300), host()->window()->bounds());

  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 300));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_270);
  EXPECT_EQ(host()->GetBoundsInPixels(), gfx::Rect(0, 0, 400, 300));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().rotation(),
            display::Display::ROTATE_270);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel(),
            gfx::Size(300, 400));
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, 300, 400));
  EXPECT_EQ(gfx::Rect(300, 400), host()->window()->bounds());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WindowTreeHostTest, HoldPointerMovesOnChildResizing) {
  aura::WindowEventDispatcher* dispatcher = host()->dispatcher();

  aura::test::WindowEventDispatcherTestApi dispatcher_api(dispatcher);

  EXPECT_FALSE(dispatcher_api.HoldingPointerMoves());

  // Signal to the ui::Compositor that a child is resizing. This will
  // immediately trigger input throttling.
  host()->compositor()->OnChildResizing();

  // Pointer moves should be throttled until the next commit. This has the
  // effect of prioritizing the resize event above other operations in aura.
  EXPECT_TRUE(dispatcher_api.HoldingPointerMoves());

  // Wait for a CompositorFrame to be activated.
  ui::DrawWaiterForTest::WaitForCompositingEnded(host()->compositor());

  // Pointer moves should be routed normally after commit.
  EXPECT_FALSE(dispatcher_api.HoldingPointerMoves());
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests if scale factor changes take effect. Previously a scale factor change
// wouldn't take effect without a bounds change. For context see
// https://crbug.com/1087626
TEST_F(WindowTreeHostTest, ShouldHandleTextScale) {
  constexpr gfx::Rect starting_bounds(
      aura::test::AuraTestHelper::kDefaultHostSize);
  auto asserter = [&](float test_scale_factor) {
    test_screen()->SetDeviceScaleFactor(test_scale_factor, false);

    EXPECT_EQ(starting_bounds, host()->GetBoundsInPixels());
    // Size should be rounded up after scaling.
    EXPECT_EQ(
        gfx::ScaleToEnclosingRect(starting_bounds, 1.0f / test_scale_factor),
        root_window()->bounds());
    EXPECT_EQ(test_scale_factor, host()->device_scale_factor());
  };

  asserter(1.0f);
  asserter(1.05f);
  asserter(1.5f);
}
#endif

TEST_F(WindowTreeHostTest, NoRewritesPostIME) {
  ui::test::TestEventRewriter event_rewriter;
  host()->AddEventRewriter(&event_rewriter);

  ui::KeyEvent key_event('A', ui::VKEY_A, ui::DomCode::NONE, 0);
  ui::EventDispatchDetails details =
      host()->GetInputMethod()->DispatchKeyEvent(&key_event);
  ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);
  EXPECT_EQ(0, event_rewriter.events_seen());

  host()->RemoveEventRewriter(&event_rewriter);
}

TEST_F(WindowTreeHostTest, ConvertDIPToPixelsShouldRespectScaleFactor) {
  const int width_in_pixels = 400;
  const int height_in_pixels = 300;
  const int width_in_dip = 200;
  const int height_in_dip = 150;

  host()->SetBoundsInPixels(gfx::Rect(0, 0, width_in_pixels, height_in_pixels));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_0);

  test_screen()->SetDeviceScaleFactor(2.f);

  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(0, 0)), gfx::Point(0, 0));
  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(width_in_dip, 0)),
            gfx::Point(width_in_pixels, 0));
  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(0, height_in_dip)),
            gfx::Point(0, height_in_pixels));
}

TEST_F(WindowTreeHostTest, ConvertDIPToPixelsShouldRespectRotation) {
  const int width_in_pixels = 400;
  const int height_in_pixels = 300;
  const int width_in_dip = 300;
  const int height_in_dip = 400;

  host()->SetBoundsInPixels(gfx::Rect(0, 0, width_in_pixels, height_in_pixels));
  test_screen()->SetDeviceScaleFactor(1.f);

  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);

  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(0, 0)),
            gfx::Point(width_in_pixels, 0));
  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(width_in_dip, 0)),
            gfx::Point(width_in_pixels, height_in_pixels));
  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(width_in_dip, height_in_dip)),
            gfx::Point(0, height_in_pixels));
  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::Point(0, height_in_dip)),
            gfx::Point(0, 0));
}

TEST_F(WindowTreeHostTest, ConvertDIPToPixelsShouldWorkWithPointF) {
  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 400));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_0);
  test_screen()->SetDeviceScaleFactor(2.f);

  EXPECT_EQ(ConvertDIPToPixels(host(), gfx::PointF(5.3f, 0)),
            gfx::PointF(10.6f, 0));
}

TEST_F(WindowTreeHostTest, ConvertPixelsToDIPShouldRespectScaleFactor) {
  const int width_in_pixels = 400;
  const int height_in_pixels = 300;
  const int width_in_dip = 200;
  const int height_in_dip = 150;

  host()->SetBoundsInPixels(gfx::Rect(0, 0, width_in_pixels, height_in_pixels));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_0);

  test_screen()->SetDeviceScaleFactor(2.f);

  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::Point(0, 0)), gfx::Point(0, 0));
  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::Point(width_in_pixels, 0)),
            gfx::Point(width_in_dip, 0));
  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::Point(0, height_in_pixels)),
            gfx::Point(0, height_in_dip));
}

TEST_F(WindowTreeHostTest, ConvertPixelsToDIPShouldRespectRotation) {
  const int width_in_pixels = 400;
  const int height_in_pixels = 300;
  const int width_in_dip = 300;
  const int height_in_dip = 400;

  host()->SetBoundsInPixels(gfx::Rect(0, 0, width_in_pixels, height_in_pixels));
  test_screen()->SetDeviceScaleFactor(1.f);

  test_screen()->SetDisplayRotation(display::Display::ROTATE_90);

  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::Point(0, 0)),
            gfx::Point(0, height_in_dip));
  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::Point(width_in_pixels, 0)),
            gfx::Point(0, 0));
  EXPECT_EQ(
      ConvertPixelsToDIP(host(), gfx::Point(width_in_pixels, height_in_pixels)),
      gfx::Point(width_in_dip, 0));
  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::Point(0, height_in_pixels)),
            gfx::Point(width_in_dip, height_in_dip));
}

TEST_F(WindowTreeHostTest, ConvertPixelsToDIPShouldWorkWithPointF) {
  host()->SetBoundsInPixels(gfx::Rect(0, 0, 400, 400));
  test_screen()->SetDisplayRotation(display::Display::ROTATE_0);
  test_screen()->SetDeviceScaleFactor(2.f);

  EXPECT_EQ(ConvertPixelsToDIP(host(), gfx::PointF(10.6f, 0)),
            gfx::PointF(5.3f, 0));
}

class TestWindow : public ui::StubWindow {
 public:
  explicit TestWindow(ui::PlatformWindowDelegate* delegate)
      : StubWindow(delegate, false, gfx::Rect(400, 600)) {}

  TestWindow(const TestWindow&) = delete;
  TestWindow& operator=(const TestWindow&) = delete;

  ~TestWindow() override {}

 private:
  // ui::StubWindow
  void Close() override {
    // It is possible for the window to receive capture-change messages during
    // destruction, for example on Windows (see crbug.com/770670).
    delegate()->OnLostCapture();
  }
};

class TestWindowTreeHost : public WindowTreeHostPlatform {
 public:
  TestWindowTreeHost() {
    SetPlatformWindow(std::make_unique<TestWindow>(this));
    CreateCompositor();
  }

  TestWindowTreeHost(const TestWindowTreeHost&) = delete;
  TestWindowTreeHost& operator=(const TestWindowTreeHost&) = delete;
};

TEST_F(WindowTreeHostTest, LostCaptureDuringTearDown) {
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kApplyNativeOcclusionToCompositor);
#endif
  TestWindowTreeHost host;
}

#if BUILDFLAG(IS_WIN)
class WindowTreeHostWithReleaseTest : public test::AuraTestBase {
 public:
  // AuraTestBase:
  void SetUp() override {
    // Disable the headless check as the bots run with CHROME_HEADLESS set.
    NativeWindowOcclusionTracker::SetHeadlessCheckEnabled(false);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kCalculateNativeWinOcclusion, {}},
            {features::kApplyNativeOcclusionToCompositor,
             {{features::kApplyNativeOcclusionToCompositorType,
               features::kApplyNativeOcclusionToCompositorTypeRelease}}},
        },
        {});
    AuraTestBase::SetUp();
  }

  void TearDown() override {
    test::AuraTestBase::TearDown();
    NativeWindowOcclusionTracker::SetHeadlessCheckEnabled(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

cc::Layer* ccLayerFromUiLayer(ui::Layer* layer) {
  return static_cast<ui::LayerAnimationDelegate*>(layer)->GetCcLayer();
}

bool WaitForFrame(WindowTreeHost* host) {
  base::RunLoop run_loop;
  bool got_frame = false;
  host->compositor()->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting([&](base::TimeTicks presentation_timestamp) {
        got_frame = true;
        run_loop.Quit();
      }));
  run_loop.Run();
  return got_frame;
}

TEST_F(WindowTreeHostWithReleaseTest, ToggleOccluded) {
  host()->Show();
  // This tests needs to drive native occlusion. If native occlusion is
  // used, it'll conflict with this test.
  NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(host());
  ASSERT_TRUE(NativeWindowOcclusionTracker::
                  IsNativeWindowOcclusionTrackingAlwaysEnabled(host()));
  cc::Layer* host_window_cc_layer =
      ccLayerFromUiLayer(host()->window()->layer());
  const cc::Layer* compositor_root_layer = host_window_cc_layer->parent();
  EXPECT_NE(nullptr, compositor_root_layer);
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  // The compositor shouldn't actually hide immediately, it needs a frame to
  // be generated.
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_EQ(nullptr, host_window_cc_layer->parent());
  ASSERT_TRUE(WaitForFrame(host()));
  EXPECT_FALSE(host()->compositor()->IsVisible());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_EQ(compositor_root_layer, host_window_cc_layer->parent());
}

TEST_F(WindowTreeHostWithReleaseTest, ShowWhileTransitioningToHidden) {
  host()->Show();
  // This tests needs to drive native occlusion. If native occlusion is
  // used, it'll conflict with this test.
  NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(host());
  ASSERT_TRUE(NativeWindowOcclusionTracker::
                  IsNativeWindowOcclusionTrackingAlwaysEnabled(host()));
  cc::Layer* host_window_cc_layer =
      ccLayerFromUiLayer(host()->window()->layer());
  const cc::Layer* compositor_root_layer = host_window_cc_layer->parent();
  EXPECT_NE(nullptr, compositor_root_layer);
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  // The compositor shouldn't actually hide immediately, it needs a frame to
  // be generated.
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_EQ(nullptr, host_window_cc_layer->parent());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_EQ(compositor_root_layer, host_window_cc_layer->parent());
}

TEST_F(WindowTreeHostWithReleaseTest, VideoCaptureLockForcesVisible) {
  ASSERT_TRUE(NativeWindowOcclusionTracker::
                  IsNativeWindowOcclusionTrackingAlwaysEnabled(host()));
  // This tests needs to drive native occlusion. If native occlusion is
  // used, it'll conflict with this test.
  NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(host());
  host()->Show();
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  ASSERT_TRUE(WaitForFrame(host()));
  EXPECT_FALSE(host()->compositor()->IsVisible());
  std::unique_ptr<WindowTreeHost::VideoCaptureLock> lock =
      host()->CreateVideoCaptureLock();
  EXPECT_TRUE(host()->compositor()->IsVisible());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  ASSERT_TRUE(WaitForFrame(host()));
  EXPECT_TRUE(host()->compositor()->IsVisible());
  lock.reset();
  ASSERT_TRUE(WaitForFrame(host()));
  EXPECT_FALSE(host()->compositor()->IsVisible());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  ASSERT_TRUE(WaitForFrame(host()));
  EXPECT_TRUE(host()->compositor()->IsVisible());
}

class WindowTreeHostWithThrottleTest : public test::AuraTestBase {
 public:
  // AuraTestBase:
  void SetUp() override {
    // Disable the headless check as the bots run with CHROME_HEADLESS set.
    NativeWindowOcclusionTracker::SetHeadlessCheckEnabled(false);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kCalculateNativeWinOcclusion, {}},
            {features::kApplyNativeOcclusionToCompositor,
             {{features::kApplyNativeOcclusionToCompositorType,
               features::kApplyNativeOcclusionToCompositorTypeThrottle}}},
        },
        {});
    AuraTestBase::SetUp();
  }

  void TearDown() override {
    test::AuraTestBase::TearDown();
    NativeWindowOcclusionTracker::SetHeadlessCheckEnabled(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WindowTreeHostWithThrottleTest, DISABLED_Basic) {
  host()->Show();
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_TRUE(test::GetThrottledHosts().empty());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_TRUE(base::Contains(test::GetThrottledHosts(), host()));
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});
  EXPECT_TRUE(test::GetThrottledHosts().empty());
  EXPECT_TRUE(host()->compositor()->IsVisible());
}

TEST_F(WindowTreeHostWithThrottleTest, DISABLED_CallHideDirectly) {
  host()->Show();
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_TRUE(test::GetThrottledHosts().empty());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  EXPECT_TRUE(host()->compositor()->IsVisible());
  EXPECT_TRUE(base::Contains(test::GetThrottledHosts(), host()));
  host()->Hide();
  EXPECT_TRUE(test::GetThrottledHosts().empty());
  EXPECT_FALSE(host()->compositor()->IsVisible());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace aura
