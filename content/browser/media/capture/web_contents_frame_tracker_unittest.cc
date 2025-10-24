// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_switches.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_feedback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen_info.h"

namespace content {
namespace {

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::StrictMock;

constexpr viz::FrameSinkId kInitSinkId(123, 456);

// Standardized screen resolutions to test common scenarios.
constexpr gfx::Size kSizeZero{0, 0};
constexpr gfx::Size kSize720p{1280, 720};
constexpr gfx::Size kSize1080p{1920, 1080};
constexpr gfx::Size kSizeWsxgaPlus{1680, 1050};

class SimpleContext : public WebContentsFrameTracker::Context {
 public:
  SimpleContext() = default;
  ~SimpleContext() override = default;

  // WebContentsFrameTracker::Context overrides.
  std::optional<gfx::Rect> GetScreenBounds() override { return screen_bounds_; }

  WebContentsImpl::CaptureTarget GetCaptureTarget() override {
    return WebContentsImpl::CaptureTarget{frame_sink_id_, gfx::NativeView{}};
  }
  void IncrementCapturerCount(const gfx::Size& capture_size) override {
    ++capturer_count_;
    last_capture_size_ = capture_size;
  }
  void DecrementCapturerCount() override { --capturer_count_; }

  void SetCaptureScaleOverride(float scale) override {
    scale_override_ = scale;
  }
  float GetCaptureScaleOverride() const override { return scale_override_; }
  int capturer_count() const { return capturer_count_; }
  const gfx::Size& last_capture_size() const { return last_capture_size_; }

  void set_frame_sink_id(viz::FrameSinkId frame_sink_id) {
    frame_sink_id_ = frame_sink_id;
  }
  void set_screen_bounds(std::optional<gfx::Rect> screen_bounds) {
    screen_bounds_ = std::move(screen_bounds);
  }
  float scale_override() const { return scale_override_; }

 private:
  int capturer_count_ = 0;
  viz::FrameSinkId frame_sink_id_ = kInitSinkId;
  gfx::Size last_capture_size_;
  std::optional<gfx::Rect> screen_bounds_;
  float scale_override_ = 1.0f;
};

// The capture device is mostly for interacting with the frame tracker. We do
// care about the frame tracker pushing back target updates, however.
class MockCaptureDevice : public WebContentsVideoCaptureDevice {
 public:
  MOCK_METHOD2(OnTargetChanged,
               void(const std::optional<viz::VideoCaptureTarget>&, uint32_t));
  MOCK_METHOD0(OnTargetPermanentlyLost, void());

  base::WeakPtr<MockCaptureDevice> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockCaptureDevice> weak_ptr_factory_{this};
};

// This test class is intentionally quite similar to
// |WebContentsVideoCaptureDevice|, and provides convenience methods for calling
// into the |WebContentsFrameTracker|, which interacts with UI thread objects
// and needs to be called carefully on the UI thread.
class WebContentsFrameTrackerTest : public RenderViewHostTestHarness {
 protected:
  WebContentsFrameTrackerTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    // The tests assume that they are running on the main thread (which is
    // equivalent to the browser's UI thread) so that they can make calls on the
    // tracker object synchronously.
    ASSERT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    // Views in the web context are incredibly fragile and prone to
    // non-deterministic test failures, so we use TestWebContents here.
    web_contents_ = TestWebContents::Create(browser_context(), nullptr);
    device_ = std::make_unique<StrictMock<MockCaptureDevice>>();

    // All tests should call target changed as part of initialization.
    EXPECT_CALL(*device_, OnTargetChanged(_, _)).Times(1);

    // This PostTask technically isn't necessary since we're already on the main
    // thread which is equivalent to the browser's UI thread, but it's a bit
    // cleaner to do so in case we want to switch to a different threading model
    // for the tests in the future.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTrackerTest::SetUpOnUIThread,
                       base::Unretained(this),
                       base::SingleThreadTaskRunner::GetCurrentDefault()));
    RunAllTasksUntilIdle();
  }

  void SetUpOnUIThread(
      const scoped_refptr<base::SequencedTaskRunner> device_task_runner) {
    auto context = std::make_unique<SimpleContext>();
    raw_context_ = context.get();
    SetScreenSize(kSize1080p);
    tracker_ = std::make_unique<WebContentsFrameTracker>(
        device_task_runner, device_->AsWeakPtr(), controller());
    tracker_->SetWebContentsAndContextForTesting(web_contents_.get(),
                                                 std::move(context));
  }

  void TearDown() override {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTrackerTest::TearDownOnUIThread,
                       base::Unretained(this)));
    RunAllTasksUntilIdle();
    RenderViewHostTestHarness::TearDown();
  }

  void TearDownOnUIThread() {
    tracker_.reset();
    device_.reset();
    web_contents_.reset();
  }

  void SetScreenSize(const gfx::Size& size) {
    raw_context_->set_screen_bounds(gfx::Rect{size});
  }

  void SetFrameSinkId(viz::FrameSinkId id) {
    raw_context_->set_frame_sink_id(id);
  }

  void StartTrackerOnUIThread(const gfx::Size& capture_size) {
    // Using base::Unretained for the tracker is presumed safe due to using
    // RunAllTasksUntilIdle in TearDown.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTracker::WillStartCapturingWebContents,
                       base::Unretained(tracker_.get()), capture_size,
                       true /* is_high_dpi_enabled */));
  }

  void StopTrackerOnUIThread() {
    // Using base::Unretained for the tracker is presumed safe due to using
    // RunAllTasksUntilIdle in TearDown.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTracker::DidStopCapturingWebContents,
                       base::Unretained(tracker_.get())));
  }

  // The controller is ignored on Android, and must be initialized on all
  // other platforms.
  MouseCursorOverlayController* controller() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return nullptr;
#else
    return &controller_;
#endif
  }
  WebContentsFrameTracker* tracker() { return tracker_.get(); }
  SimpleContext* context() { return raw_context_; }
  StrictMock<MockCaptureDevice>* device() { return device_.get(); }

 private:
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MouseCursorOverlayController controller_;
#endif

  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<StrictMock<MockCaptureDevice>> device_;
  std::unique_ptr<WebContentsFrameTracker> tracker_;

  // Save because the pointed-to location should not change during testing.
  raw_ptr<SimpleContext, AcrossTasksDanglingUntriaged> raw_context_;
};

TEST_F(WebContentsFrameTrackerTest, CalculatesPreferredSizeClampsToView) {
  SetScreenSize(kSize720p);
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize1080p));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeNoLargerThanCaptureSize) {
  SetScreenSize(kSize1080p);
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));
  EXPECT_EQ(kSize1080p, tracker()->CalculatePreferredSize(kSize1080p));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeWithCorrectAspectRatio) {
  SetScreenSize(kSizeWsxgaPlus);

  // 720P is strictly less than WSXGA+, so should be unchanged.
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));

  // 1080P is larger, so should be scaled appropriately.
  EXPECT_EQ((gfx::Size{1680, 945}),
            tracker()->CalculatePreferredSize(kSize1080p));

  // Wider capture size.
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1050}));

  // Taller capture size.
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2100}));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeAspectRatioWithNoOffByOneErrors) {
  SetScreenSize(kSizeWsxgaPlus);

  // Wider capture size.
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1050}));
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1051}));
  EXPECT_EQ((gfx::Size{1680, 526}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1052}));
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3361, 1052}));
  EXPECT_EQ((gfx::Size{1680, 666}),
            tracker()->CalculatePreferredSize(gfx::Size{5897, 2339}));

  // Taller capture size.
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2100}));
  EXPECT_EQ((gfx::Size{499, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2101}));
  EXPECT_EQ((gfx::Size{499, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2102}));
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1001, 2102}));
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1002, 2102}));

  // Some larger and prime factor cases to sanity check.
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21841, 7351}));
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21841, 7349}));
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21839, 7351}));
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21839, 7349}));

  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139441, 55651}));
  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139439, 55651}));
  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139441, 55649}));
  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139439, 55649}));

  // Finally, just check for roundoff errors.
  SetScreenSize(gfx::Size{1000, 1000});
  EXPECT_EQ((gfx::Size{1000, 333}),
            tracker()->CalculatePreferredSize(gfx::Size{3000, 1000}));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeLeavesCaptureSizeIfSmallerThanScreen) {
  // Smaller in both directions, but different aspect ratio, should be
  // unchanged.
  SetScreenSize(kSize1080p);
  EXPECT_EQ(kSizeWsxgaPlus, tracker()->CalculatePreferredSize(kSizeWsxgaPlus));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeWithZeroValueProperly) {
  // If a capture dimension is zero, the preferred size should be (0, 0).
  EXPECT_EQ((kSizeZero), tracker()->CalculatePreferredSize(gfx::Size{0, 1000}));
  EXPECT_EQ((kSizeZero), tracker()->CalculatePreferredSize(kSizeZero));
  EXPECT_EQ((kSizeZero), tracker()->CalculatePreferredSize(gfx::Size{1000, 0}));

  // If a screen dimension is zero, the preferred size should be (0, 0). This
  // probably means the tab isn't being drawn anyway.
  SetScreenSize(gfx::Size{1920, 0});
  EXPECT_EQ(kSizeZero, tracker()->CalculatePreferredSize(kSize720p));
  SetScreenSize(gfx::Size{0, 1080});
  EXPECT_EQ(kSizeZero, tracker()->CalculatePreferredSize(kSize720p));
  SetScreenSize(kSizeZero);
  EXPECT_EQ(kSizeZero, tracker()->CalculatePreferredSize(kSize720p));
}

TEST_F(WebContentsFrameTrackerTest, UpdatesPreferredSizeOnWebContents) {
  StartTrackerOnUIThread(kSize720p);
  RunAllTasksUntilIdle();

  // In this case, the capture size requested is smaller than the screen size,
  // so it should be used.
  EXPECT_EQ(kSize720p, context()->last_capture_size());
  EXPECT_EQ(context()->capturer_count(), 1);

  // When we stop the tracker, the web contents issues a preferred size change
  // of the "old" size--so it shouldn't change.
  StopTrackerOnUIThread();
  RunAllTasksUntilIdle();
  EXPECT_EQ(kSize720p, context()->last_capture_size());
  EXPECT_EQ(context()->capturer_count(), 0);
}

TEST_F(WebContentsFrameTrackerTest, NotifiesOfLostTargets) {
  EXPECT_CALL(*device(), OnTargetPermanentlyLost()).Times(1);
  tracker()->WebContentsDestroyed();
  RunAllTasksUntilIdle();
}

// We test target changing for all other tests as part of set up, but also
// test the observer callbacks here.
TEST_F(WebContentsFrameTrackerTest, NotifiesOfTargetChanges) {
  const viz::FrameSinkId kNewId(42, 1337);
  SetFrameSinkId(kNewId);
  EXPECT_CALL(
      *device(),
      OnTargetChanged(std::make_optional<viz::VideoCaptureTarget>(kNewId),
                      /*sub_capture_target_version=*/0))
      .Times(1);

  // The tracker doesn't actually use the frame host information, just
  // posts a possible target change.
  tracker()->RenderFrameHostChanged(nullptr, nullptr);
  RunAllTasksUntilIdle();
}

TEST_F(WebContentsFrameTrackerTest,
       CroppingChangesTargetParametersAndInvokesCallback) {
  const base::Token kCropId(19831230, 19840730);

  // Expect the callback handed to Crop() to be invoke with kSuccess.
  bool success = false;
  base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)> callback =
      base::BindOnce(
          [](bool* success, media::mojom::ApplySubCaptureTargetResult result) {
            *success =
                (result == media::mojom::ApplySubCaptureTargetResult::kSuccess);
          },
          &success);

  // Expect OnTargetChanged() to be invoked once with the crop-ID.
  EXPECT_CALL(*device(),
              OnTargetChanged(std::make_optional<viz::VideoCaptureTarget>(
                                  kInitSinkId, kCropId),
                              /*sub_capture_target_version=*/1))
      .Times(1);

  tracker()->ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType::kCropTarget, kCropId,
      /*sub_capture_target_version=*/1, std::move(callback));

  RunAllTasksUntilIdle();
  EXPECT_TRUE(success);
}

}  // namespace
}  // namespace content
