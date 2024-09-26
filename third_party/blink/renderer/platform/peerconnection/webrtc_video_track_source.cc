// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace {

gfx::Rect CropRectangle(const gfx::Rect& input_rect,
                        const gfx::Rect& cropping_rect) {
  gfx::Rect result(input_rect);
  result.Intersect(cropping_rect);
  result.Offset(-cropping_rect.x(), -cropping_rect.y());
  if (result.x() < 0)
    result.set_x(0);
  if (result.y() < 0)
    result.set_y(0);
  return result;
}

gfx::Rect ScaleRectangle(const gfx::Rect& input_rect,
                         gfx::Size original,
                         gfx::Size scaled) {
  if (input_rect.IsEmpty()) {
    return input_rect;
  }
  gfx::Rect result;
  // Rounded down.
  result.set_x(input_rect.x() * scaled.width() / original.width());
  result.set_y(input_rect.y() * scaled.height() / original.height());
  // rounded up.
  result.set_width(input_rect.width() * scaled.width() / original.width());
  result.set_height(input_rect.height() * scaled.height() / original.height());
  // Snap to 2x2 grid because of UV subsampling.
  if (result.x() % 2) {
    result.set_x(result.x() - 1);
    result.set_width(result.width() + 1);
  }
  if (result.y() % 2) {
    result.set_y(result.y() - 1);
    result.set_height(result.height() + 1);
  }
  if (result.width() % 2) {
    result.set_width(result.width() + 1);
  }
  if (result.height() % 2) {
    result.set_height(result.height() + 1);
  }
  // Expand the rect by 2 pixels in each direction, to include any possible
  // scaling artifacts.
  result.set_x(result.x() - 2);
  result.set_y(result.y() - 2);
  result.set_width(result.width() + 4);
  result.set_height(result.height() + 4);
  result.Intersect(gfx::Rect(0, 0, scaled.width(), scaled.height()));
  return result;
}

webrtc::VideoRotation GetFrameRotation(const media::VideoFrame* frame) {
  if (!frame->metadata().transformation) {
    return webrtc::kVideoRotation_0;
  }
  switch (frame->metadata().transformation->rotation) {
    case media::VIDEO_ROTATION_0:
      return webrtc::kVideoRotation_0;
    case media::VIDEO_ROTATION_90:
      return webrtc::kVideoRotation_90;
    case media::VIDEO_ROTATION_180:
      return webrtc::kVideoRotation_180;
    case media::VIDEO_ROTATION_270:
      return webrtc::kVideoRotation_270;
    default:
      return webrtc::kVideoRotation_0;
  }
}

}  // anonymous namespace

namespace blink {

WebRtcVideoTrackSource::WebRtcVideoTrackSource(
    bool is_screencast,
    absl::optional<bool> needs_denoising,
    media::VideoCaptureFeedbackCB feedback_callback,
    base::RepeatingClosure request_refresh_frame_callback,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : AdaptedVideoTrackSource(/*required_alignment=*/1),
      adapter_resources_(
          new WebRtcVideoFrameAdapter::SharedResources(gpu_factories)),
      is_screencast_(is_screencast),
      needs_denoising_(needs_denoising),
      feedback_callback_(std::move(feedback_callback)),
      request_refresh_frame_callback_(
          std::move(request_refresh_frame_callback)) {
  DETACH_FROM_THREAD(thread_checker_);
}

WebRtcVideoTrackSource::~WebRtcVideoTrackSource() = default;

void WebRtcVideoTrackSource::SetCustomFrameAdaptationParamsForTesting(
    const FrameAdaptationParams& params) {
  custom_frame_adaptation_params_for_testing_ = params;
}

void WebRtcVideoTrackSource::SetSinkWantsForTesting(
    const rtc::VideoSinkWants& sink_wants) {
  video_adapter()->OnSinkWants(sink_wants);
}

WebRtcVideoTrackSource::SourceState WebRtcVideoTrackSource::state() const {
  // TODO(nisse): What's supposed to change this state?
  return MediaSourceInterface::SourceState::kLive;
}

bool WebRtcVideoTrackSource::remote() const {
  return false;
}

bool WebRtcVideoTrackSource::is_screencast() const {
  return is_screencast_;
}

absl::optional<bool> WebRtcVideoTrackSource::needs_denoising() const {
  return needs_denoising_;
}

void WebRtcVideoTrackSource::RequestRefreshFrame() {
  request_refresh_frame_callback_.Run();
}

void WebRtcVideoTrackSource::SendFeedback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (feedback_callback_.is_null()) {
    return;
  }
  media::VideoCaptureFeedback feedback;
  feedback.max_pixels = video_adapter()->GetTargetPixels();
  feedback.max_framerate_fps = video_adapter()->GetMaxFramerate();
  feedback.Combine(adapter_resources_->GetFeedback());
  feedback_callback_.Run(feedback);
}

void WebRtcVideoTrackSource::OnFrameCaptured(
    scoped_refptr<media::VideoFrame> frame,
    std::vector<scoped_refptr<media::VideoFrame>> scaled_frames) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("media", "WebRtcVideoSource::OnFrameCaptured");
  if (!CanConvertToWebRtcVideoFrameBuffer(frame.get())) {
    // Since connecting sources and sinks do not check the format, we need to
    // just ignore formats that we can not handle.
    LOG(ERROR) << "We cannot send frame with storage type: "
               << frame->AsHumanReadableString();
    NOTREACHED();
    return;
  }

  SendFeedback();

  // Compute what rectangular region has changed since the last frame
  // that we successfully delivered to the base class method
  // rtc::AdaptedVideoTrackSource::OnFrame(). This region is going to be
  // relative to the coded frame data, i.e.
  // [0, 0, frame->coded_size().width(), frame->coded_size().height()].
  absl::optional<int> capture_counter = frame->metadata().capture_counter;
  absl::optional<gfx::Rect> update_rect = frame->metadata().capture_update_rect;

  const bool has_valid_update_rect =
      update_rect.has_value() && capture_counter.has_value() &&
      previous_capture_counter_.has_value() &&
      (*capture_counter == (*previous_capture_counter_ + 1));
  DVLOG(3) << "has_valid_update_rect = " << has_valid_update_rect;
  if (capture_counter)
    previous_capture_counter_ = capture_counter;
  if (has_valid_update_rect) {
    if (!accumulated_update_rect_) {
      accumulated_update_rect_ = update_rect;
    } else {
      accumulated_update_rect_->Union(*update_rect);
    }
  } else {
    accumulated_update_rect_ = absl::nullopt;
  }

  if (accumulated_update_rect_) {
    DVLOG(3) << "accumulated_update_rect_ = [" << accumulated_update_rect_->x()
             << ", " << accumulated_update_rect_->y() << ", "
             << accumulated_update_rect_->width() << ", "
             << accumulated_update_rect_->height() << "]";
  }

  // Calculate desired target cropping and scaling of the received frame. Note,
  // that the frame may already have some cropping and scaling soft-applied via
  // |frame->visible_rect()| and |frame->natural_size()|. The target cropping
  // and scaling computed by AdaptFrame() below is going to be applied on top
  // of the existing one.
  const int orig_width = frame->natural_size().width();
  const int orig_height = frame->natural_size().height();
  const int64_t now_us = rtc::TimeMicros();
  FrameAdaptationParams frame_adaptation_params =
      ComputeAdaptationParams(orig_width, orig_height, now_us);
  if (frame_adaptation_params.should_drop_frame)
    return;

  const int64_t translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(frame->timestamp().InMicroseconds(),
                                            now_us);

  absl::optional<webrtc::Timestamp> capture_time_identifier;
  // Set |capture_time_identifier| only when frame->timestamp() is a valid
  // value (infinite values are invalid).
  if (!frame->timestamp().is_inf()) {
    capture_time_identifier =
        webrtc::Timestamp::Micros(frame->timestamp().InMicroseconds());
  }

  // Translate the |crop_*| values output by AdaptFrame() from natural size to
  // visible size. This is needed to apply the new cropping on top of any
  // existing soft-applied cropping and scaling when using
  // media::VideoFrame::WrapVideoFrame().
  gfx::Rect cropped_visible_rect(
      frame->visible_rect().x() + frame_adaptation_params.crop_x *
                                      frame->visible_rect().width() /
                                      orig_width,
      frame->visible_rect().y() + frame_adaptation_params.crop_y *
                                      frame->visible_rect().height() /
                                      orig_height,
      frame_adaptation_params.crop_width * frame->visible_rect().width() /
          orig_width,
      frame_adaptation_params.crop_height * frame->visible_rect().height() /
          orig_height);

  DVLOG(3) << "cropped_visible_rect = "
           << "[" << cropped_visible_rect.x() << ", "
           << cropped_visible_rect.y() << ", " << cropped_visible_rect.width()
           << ", " << cropped_visible_rect.height() << "]";

  const gfx::Size adapted_size(frame_adaptation_params.scale_to_width,
                               frame_adaptation_params.scale_to_height);
  // Soft-apply the new (combined) cropping and scaling.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapVideoFrame(frame, frame->format(),
                                        cropped_visible_rect, adapted_size);
  if (!video_frame)
    return;

  // The webrtc::VideoFrame::UpdateRect expected by WebRTC must be
  // relative to the |visible_rect()|. We need to translate.
  if (accumulated_update_rect_) {
    accumulated_update_rect_ =
        CropRectangle(*accumulated_update_rect_, frame->visible_rect());
  }

  // If no scaling is needed, return a wrapped version of |frame| directly.
  // The soft-applied cropping will be taken into account by the remainder
  // of the pipeline.
  if (video_frame->natural_size() == video_frame->visible_rect().size()) {
    DeliverFrame(std::move(video_frame), std::move(scaled_frames),
                 base::OptionalToPtr(accumulated_update_rect_),
                 translated_camera_time_us, capture_time_identifier);
    return;
  }

  if (accumulated_update_rect_) {
    accumulated_update_rect_ = ScaleRectangle(
        *accumulated_update_rect_, video_frame->visible_rect().size(),
        video_frame->natural_size());
  }

  DeliverFrame(std::move(video_frame), std::move(scaled_frames),
               base::OptionalToPtr(accumulated_update_rect_),
               translated_camera_time_us, capture_time_identifier);
}

void WebRtcVideoTrackSource::OnNotifyFrameDropped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  OnFrameDropped();
}

WebRtcVideoTrackSource::FrameAdaptationParams
WebRtcVideoTrackSource::ComputeAdaptationParams(int width,
                                                int height,
                                                int64_t time_us) {
  if (custom_frame_adaptation_params_for_testing_.has_value())
    return custom_frame_adaptation_params_for_testing_.value();

  FrameAdaptationParams result{false, 0, 0, 0, 0, 0, 0};
  result.should_drop_frame = !AdaptFrame(
      width, height, time_us, &result.scale_to_width, &result.scale_to_height,
      &result.crop_width, &result.crop_height, &result.crop_x, &result.crop_y);
  return result;
}

void WebRtcVideoTrackSource::DeliverFrame(
    scoped_refptr<media::VideoFrame> frame,
    std::vector<scoped_refptr<media::VideoFrame>> scaled_frames,
    gfx::Rect* update_rect,
    int64_t timestamp_us,
    absl::optional<webrtc::Timestamp> capture_time_identifier) {
  if (update_rect) {
    DVLOG(3) << "update_rect = "
             << "[" << update_rect->x() << ", " << update_rect->y() << ", "
             << update_rect->width() << ", " << update_rect->height() << "]";
  }

  // If the cropping or the size have changed since the previous
  // frame, even if nothing in the incoming coded frame content has changed, we
  // have to assume that every pixel in the outgoing frame has changed.
  if (frame->visible_rect() != cropping_rect_of_previous_delivered_frame_ ||
      frame->natural_size() != natural_size_of_previous_delivered_frame_) {
    cropping_rect_of_previous_delivered_frame_ = frame->visible_rect();
    natural_size_of_previous_delivered_frame_ = frame->natural_size();
    update_rect = nullptr;
  }

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame, std::move(scaled_frames), adapter_resources_));

  webrtc::VideoFrame::Builder frame_builder =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(frame_adapter)
          .set_rotation(GetFrameRotation(frame.get()))
          .set_timestamp_us(timestamp_us)
          .set_capture_time_identifier(capture_time_identifier);
  if (update_rect) {
    frame_builder.set_update_rect(webrtc::VideoFrame::UpdateRect{
        update_rect->x(), update_rect->y(), update_rect->width(),
        update_rect->height()});
  }
  OnFrame(frame_builder.build());

  // Clear accumulated_update_rect_.
  accumulated_update_rect_ = gfx::Rect();
}

}  // namespace blink
