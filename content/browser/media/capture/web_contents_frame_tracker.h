// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_FRAME_TRACKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_FRAME_TRACKER_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_feedback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContentsVideoCaptureDevice;
class MouseCursorOverlayController;
class RenderFrameHost;

// Monitors the WebContents instance and notifies the parent
// WebContentsVideoCaptureDevice |device| class any time the frame sink or
// main render frame's view changes.
class CONTENT_EXPORT WebContentsFrameTracker final
    : public WebContentsObserver {
 public:
  // We generally retrieve certain properties by accessing fields on the
  // WebContents object, however these properties may come from a different
  // context in some circumstances, such as testing.
  class Context {
   public:
    virtual ~Context() = default;

    // Get bounds of the attached screen, if any.
    virtual absl::optional<gfx::Rect> GetScreenBounds() = 0;

    // While the DOM always has a FrameSinkId, we may want to capture
    // a different frame sink ID overlaying the DOM content that represents
    // what we actually want to capture.
    virtual viz::FrameSinkId GetFrameSinkIdForCapture() = 0;

    // Capturer count handling is tricky in testing, since setting it
    // on the web contents uses a view even though the view may not be
    // initialized in the test harness.
    virtual void IncrementCapturerCount(const gfx::Size& capture_size) = 0;
    virtual void DecrementCapturerCount() = 0;

    // Adjust the associated RenderWidgetHostView's rendering scale for capture.
    virtual void SetScaleOverrideForCapture(float scale) = 0;
    virtual float GetScaleOverrideForCapture() const = 0;
  };

  // The |device| weak pointer will be used to post tasks back to the device via
  // |device_task_runner|.
  //
  // See the cursor_controller_ member comments for cursor_controller lifetime
  // documentation.
  WebContentsFrameTracker(
      scoped_refptr<base::SequencedTaskRunner> device_task_runner,
      base::WeakPtr<WebContentsVideoCaptureDevice> device,
      MouseCursorOverlayController* cursor_controller);

  WebContentsFrameTracker(WebContentsFrameTracker&&) = delete;
  WebContentsFrameTracker(const WebContentsFrameTracker&) = delete;
  WebContentsFrameTracker& operator=(const WebContentsFrameTracker&&) = delete;
  WebContentsFrameTracker& operator=(const WebContentsFrameTracker&) = delete;

  ~WebContentsFrameTracker() override;

  void WillStartCapturingWebContents(const gfx::Size& capture_size,
                                     bool is_high_dpi_enabled);
  void DidStopCapturingWebContents();

  void SetCapturedContentSize(const gfx::Size& content_size);

  // The preferred size calculated here is a strong suggestion to UI
  // layout code to size the viewport such that physical rendering matches the
  // exact capture size. This helps to eliminate redundant scaling operations
  // during capture. Note that if there are multiple capturers, a "first past
  // the post" system is used and the first capturer's preferred size is set.
  gfx::Size CalculatePreferredSize(const gfx::Size& capture_size);

  // Determines the preferred DPI scaling factor based on the current content
  // size of the video frame, meaning the populated pixels, and the unscaled
  // current content size, meaning the original size of the frame before scaling
  // was applied to fit the frame. These values are used to compare against
  // the currently requested |capture_size_| set in
  // |WillStartCapturingWebContents()|.
  float CalculatePreferredScaleFactor(
      const gfx::Size& current_content_size,
      const gfx::Size& unscaled_current_content_size);

  // Called whenever the capture device gets updated feedback.
  //
  // NOTE: the way this report gets applied in this class assumes that it is
  // updated relatively infrequently (e.g. multiple seconds between reports). If
  // we find that this is called more frequently the algorithm should be updated
  // to weigh historic reports as well as the last received feedback.
  void OnUtilizationReport(media::VideoCaptureFeedback feedback);

  // WebContentsObserver overrides.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void WebContentsDestroyed() override;
  void CaptureTargetChanged() override;

  void SetWebContentsAndContextFromRoutingId(const GlobalRenderFrameHostId& id);

  // Start/stop cropping.
  //
  // Must only be called on the UI thread.
  //
  // Non-empty |crop_id| sets (or changes) the crop-target.
  // Empty |crop_id| reverts the capture to its original, uncropped state.
  //
  // |crop_version| must be incremented by at least one for each call.
  // By including it in frame's metadata, Viz informs Blink what was the
  // latest invocation of cropTo() before a given frame was produced.
  //
  // The callback reports success/failure. The callback may be called on an
  // arbitrary sequence, so the caller is responsible for re-posting it
  // to the desired target sequence as necessary.
  void Crop(const base::Token& crop_id,
            uint32_t crop_version,
            base::OnceCallback<void(media::mojom::CropRequestResult)> callback);

  // WebContents are retrieved on the UI thread normally, from the render IDs,
  // so this method is provided for tests to set the web contents directly.
  void SetWebContentsAndContextForTesting(WebContents* web_contents,
                                          std::unique_ptr<Context> context);

 private:
  // Re-evaluates whether a new frame sink should be targeted for capture and
  // notifies the device. If the WebContents instance is no longer being
  // observed, the device is notified that the capture target has been
  // permanently lost.
  void OnPossibleTargetChange();

  // Sets the target view for the cursor controller on non-Android platforms.
  // Noop on Android.
  void SetTargetView(gfx::NativeView view);

  // Helper for setting the capture scale override, should always update the
  // context at the same time. NOTE: `new_value`
  void SetCaptureScaleOverride(float new_value);

  // Helper for applying the latest-received utilization feedback to throttle
  // the capture scale override, if necessary.
  float DetermineMaxScaleOverride();

  // The maximum capture scale override.
  static const float kMaxCaptureScaleOverride;

  // |device_| may be dereferenced only by tasks run by |device_task_runner_|.
  const base::WeakPtr<WebContentsVideoCaptureDevice> device_;

  // The task runner to be used for device callbacks.
  const scoped_refptr<base::SequencedTaskRunner> device_task_runner_;

  // Owned by FrameSinkVideoCaptureDevice. This will be valid for the life of
  // WebContentsFrameTracker because the WebContentsFrameTracker deleter task
  // will be posted to the UI thread before the MouseCursorOverlayController
  // deleter task.
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MouseCursorOverlayController, DanglingUntriaged> cursor_controller_ =
      nullptr;
#endif

  // We may not have a frame sink ID target at all times.
  std::unique_ptr<Context> context_;
  viz::FrameSinkId target_frame_sink_id_;
  base::Token crop_id_;
  gfx::NativeView target_native_view_ = gfx::NativeView();

  // Indicates whether the WebContents's capturer count needs to be
  // decremented.
  bool is_capturing_ = false;

  // Whenever the crop-target of a stream changes, the associated crop-version
  // is incremented. This value is used in frames' metadata so as to allow
  // other modules (mostly Blink) to see which frames are cropped to the
  // old/new specified crop-target.
  //
  // The value 0 is used before any crop-target is assigned. (Note that by
  // cropping and then uncropping, values other than 0 can also be associated
  // with an uncropped track.)
  uint32_t crop_version_ = 0;

  // Scale multiplier used for the captured content when HiDPI capture mode is
  // active. A value of 1.0 means no override, using the original unmodified
  // resolution. The scale override is a multiplier applied to both the X and Y
  // dimensions, so a value of 2.0 means four times the pixel count. This value
  // tracks the intended scale according to the heuristic. Whenever the value
  // changes, the new scale is immediately applied to the RenderWidgetHostView
  // via SetScaleOverrideForCapture. The value is also saved in this attribute
  // so that it can be undone and/or re-applied when the RenderFrameHost
  // changes.
  //
  // NOTE: the value provided to the captured content is the min of this
  // property and `max_capture_scale_override_`.
  float desired_capture_scale_override_ = 1.0f;

  // Track the number of times the capture scale override mutates in a single
  // session.
  int scale_override_change_count_ = 0;

  // The maximum capture scale override, based on the last received
  // `capture_feedback_`.
  float max_capture_scale_override_ = kMaxCaptureScaleOverride;

  // The last reported content size, if any.
  absl::optional<gfx::Size> content_size_;

  // The last received video capture feedback, if any.
  absl::optional<media::VideoCaptureFeedback> capture_feedback_;

  // The consumer-requested capture size, set in |WillStartCapturingWebContents|
  // to indicate the preferred frame size from the video frame consumer. Note
  // that frames will not necessarily be this size due to a variety of reasons,
  // so the |current_content_size| passed into |CalculatePreferredScaleFactor|
  // may differ from this value.
  gfx::Size capture_size_;

  // When false, effectively disables HiDPI capture mode by making
  // CalculatePreferredScaleFactor always return 1.0f.
  bool is_high_dpi_enabled_ = true;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_FRAME_TRACKER_H_
