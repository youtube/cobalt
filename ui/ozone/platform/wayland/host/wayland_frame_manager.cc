// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"

#include <presentation-time-client-protocol.h>
#include <sync/sync.h>
#include <cstdint>

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_set.h"
#include "base/time/time.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

namespace {

constexpr uint32_t kMaxNumberOfFrames = 20u;
constexpr uint32_t kMaxFramesInFlight = 3u;

constexpr base::TimeDelta kPresentationFlushTimerDuration =
    base::Milliseconds(160);
constexpr base::TimeDelta kPresentationFlushTimerStopThreshold =
    kPresentationFlushTimerDuration / 10;

constexpr char kBoundsRectNanOrInf[] =
    "Overlay bounds_rect is invalid (NaN or infinity).";

bool potential_compositor_buffer_lock = true;

bool ValidateRect(const gfx::RectF& rect) {
  return !std::isnan(rect.x()) && !std::isnan(rect.y()) &&
         !std::isnan(rect.width()) && !std::isnan(rect.height()) &&
         !std::isinf(rect.x()) && !std::isinf(rect.y()) &&
         !std::isinf(rect.width()) && !std::isinf(rect.height());
}

uint32_t GetPresentationKindFlags(uint32_t flags) {
  // Wayland spec has different meaning of VSync. In Chromium, VSync means to
  // update the begin frame vsync timing based on presentation feedback.
  uint32_t presentation_flags = gfx::PresentationFeedback::kVSync;

  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK)
    presentation_flags |= gfx::PresentationFeedback::kHWClock;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION)
    presentation_flags |= gfx::PresentationFeedback::kHWCompletion;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY)
    presentation_flags |= gfx::PresentationFeedback::kZeroCopy;

  return presentation_flags;
}

}  // namespace

WaylandFrame::WaylandFrame(
    uint32_t frame_id,
    int64_t seq,
    WaylandSurface* root_surface,
    wl::WaylandOverlayConfig root_config,
    base::circular_deque<
        std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
        subsurfaces_to_overlays)
    : frame_id(frame_id),
      root_surface(root_surface),
      root_config(std::move(root_config)),
      subsurfaces_to_overlays(std::move(subsurfaces_to_overlays)),
      submission_acked(false),
      presentation_acked(false),
      seq(seq) {}

WaylandFrame::WaylandFrame(
    WaylandSurface* root_surface,
    wl::WaylandOverlayConfig root_config,
    base::circular_deque<
        std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
        subsurfaces_to_overlays)
    : root_surface(root_surface),
      root_config(std::move(root_config)),
      subsurfaces_to_overlays(std::move(subsurfaces_to_overlays)),
      submission_acked(true),
      presentation_acked(true) {}

WaylandFrame::~WaylandFrame() = default;

WaylandFrameManager::WaylandFrameManager(WaylandWindow* window,
                                         WaylandConnection* connection)
    : window_(window), connection_(connection), weak_factory_(this) {
  if (!connection->zaura_shell() ||
      connection->zaura_shell()->HasBugFix(1358908)) {
    // TODO(msisov): if this gets removed at some point, the
    // WaylandSurfaceFactoryTest can also stop sending this bug fix.
    potential_compositor_buffer_lock = false;
  }
}

WaylandFrameManager::~WaylandFrameManager() {
  ClearStates();
}

void WaylandFrameManager::RecordFrame(std::unique_ptr<WaylandFrame> frame) {
  DCHECK_LE(pending_frames_.size(), 6u);

  bool buffer_pending_creation = false;
  // The |frame| may have buffers to be sent for submission, which might not
  // have been created yet. This must be done now if they cannot be created
  // immediately. Thus, dispatch this request so that buffers are created by the
  // time this frame is played back if |pending_frames_| is not empty.
  // Otherwise, there is no point to ensure wl_buffers exist as
  // MaybeProcessPendingFrame will do that as well.
  if (!connection_->buffer_factory()->CanCreateDmabufImmed() &&
      !pending_frames_.empty()) {
    buffer_pending_creation =
        EnsureWlBuffersExist(*frame) && !frame->buffer_lost;
  }

  pending_frames_.push_back(std::move(frame));
  // There are wl_buffers missing, need to wait. MaybeProcessPendingFrame will
  // be called as soon as buffers are created.
  if (!buffer_pending_creation)
    MaybeProcessPendingFrame();
}

void WaylandFrameManager::MaybeProcessPendingFrame() {
  if (pending_frames_.empty())
    return;

  auto* frame = pending_frames_.front().get();
  DCHECK(frame) << "This WaylandFrame is already in playback.";
  if (!frame)
    return;

  // Frame callback hasn't been acked, need to wait.
  if (!submitted_frames_.empty() &&
      submitted_frames_.back()->wl_frame_callback) {
    return;
  }

  // Window is still neither configured nor has pending configure bounds, need
  // to wait. Probably happens only in early stages of window initialization.
  if (!window_->received_configure_event())
    return;

  // Ensure wl_buffer existence. This is called for the first time in the
  // following cases:
  // 1) if it is possible to create buffers immediately to ensure
  // WaylandBufferHandles are not lost and to create wl_buffers if they have not
  // existed yet (a new buffer is submitted).
  // 2) or |pending_frames| was empty when RecordFrame for this |frame| was
  // called regardless whether it is possible to create wl_buffers immediately
  // or not.
  const bool has_buffer_pending_creation = EnsureWlBuffersExist(*frame);
  // There are wl_buffers missing, need to wait.
  if (has_buffer_pending_creation && !frame->buffer_lost) {
    DLOG_IF(FATAL, has_buffer_pending_creation &&
                       connection_->buffer_factory()->CanCreateDmabufImmed())
        << "Buffers should have been created immediately.";
    return;
  }

  // If processing a valid frame, update window's visual size, which may result
  // in surface configuration being done, i.e: xdg_surface set_window_geometry +
  // ack_configure requests being issued.
  const wl::WaylandOverlayConfig& config = frame->root_config;
  if (!frame->buffer_lost && !!config.buffer_id) {
    if (!ValidateRect(config.bounds_rect)) {
      fatal_error_message_ = kBoundsRectNanOrInf;
    } else {
      window_->OnSequencePoint(frame->seq);
      // During a tab dragging session, OnSequencePoint() can implicitly invoke
      // Hide(). |pending_frames_| will be cleared and we should return
      // directly.
      if (pending_frames_.empty())
        return;
    }
  }

  // Skip this frame if:
  // 1. It can't be submitted due to lost buffers.
  // 2. Even after updating visual size above, |window_| is still not fully
  //    configured, which might mean that the current frame sent by the gpu
  //    is still out-of-sync with the pending configure sequences received from
  //    the Wayland compositor. This avoids protocol errors as observed in
  //    https://crbug.com/1313023.
  // 3. A fatal error message has been set.
  if (!fatal_error_message_.empty() || frame->buffer_lost ||
      !window_->IsSurfaceConfigured())
    DiscardFrame(std::move(pending_frames_.front()));
  else
    PlayBackFrame(std::move(pending_frames_.front()));

  pending_frames_.pop_front();

  if (!fatal_error_message_.empty()) {
    connection_->buffer_manager_host()->OnCommitOverlayError(
        fatal_error_message_);
    return;
  }

  // wl_frame_callback drives the continuous playback of frames, if the frame we
  // just played-back did not set up a wl_frame_callback, we should playback
  // another frame.
  if (!submitted_frames_.empty() &&
      !submitted_frames_.back()->wl_frame_callback) {
    MaybeProcessPendingFrame();
  }
}

void WaylandFrameManager::PlayBackFrame(std::unique_ptr<WaylandFrame> frame) {
  DCHECK(!frame->buffer_lost);
  DCHECK(window_->IsSurfaceConfigured());

  auto* root_surface = frame->root_surface.get();
  auto& root_config = frame->root_config;
  bool empty_frame = !root_config.buffer_id;

  // Configure subsurfaces. Traverse the deque backwards s.t. we can set
  // frame_callback and presentation_feedback on the top-most possible surface.
  WaylandSubsurface* reference_above = nullptr;
  for (auto& [subsurface, config] :
       base::Reversed(frame->subsurfaces_to_overlays)) {
    DCHECK(subsurface);
    auto* surface = subsurface->wayland_surface();
    if (empty_frame || !config.buffer_id ||
        wl_fixed_from_double(config.opacity) == 0) {
      // If the subsurface has already been hidden, there is not point to
      // hide that again and traverse through submitted_buffers.
      if (!subsurface->IsVisible())
        continue;

      subsurface->Hide();
      // Mutter sometimes does not call buffer.release if wl_surface role is
      // destroyed, causing graphics freeze. Manually release buffer from the
      // last frame and trigger OnSubmission callbacks.
      if (!submitted_frames_.empty()) {
        auto result = submitted_frames_.back()->submitted_buffers.find(surface);
        if (result != submitted_frames_.back()->submitted_buffers.end()) {
          submitted_frames_.back()->submitted_buffers.erase(result);
          surface->AttachBuffer(nullptr);
          surface->ApplyPendingState();
          surface->Commit(false);
        }
      }
    } else {
      subsurface->ConfigureAndShowSurface(
          config.bounds_rect, root_config.bounds_rect, config.clip_rect,
          root_config.surface_scale_factor, nullptr, reference_above);
      ApplySurfaceConfigure(frame.get(), surface, config, true);
      // A fatal error happened. Must stop the playback and terminate the gpu
      // process as it might have been compromised.
      if (!fatal_error_message_.empty())
        return;
      reference_above = subsurface;
      surface->Commit(false);
    }
  }

  DCHECK(fatal_error_message_.empty());

  if (empty_frame) {
    // GPU channel has been destroyed. Do nothing for empty frames except that
    // the frame should be marked as failed if it hasn't been presented yet.
    if (!frame->presentation_acked)
      frame->feedback = gfx::PresentationFeedback::Failure();
  } else {
    // Opaque region is set during OnSequencePoint() no need to set it again.
    ApplySurfaceConfigure(frame.get(), root_surface, root_config, false);
    // A fatal error happened. Must stop the playback and terminate the gpu
    // process as it might have been compromised.
    if (!fatal_error_message_.empty())
      return;
  }

  DCHECK(empty_frame || !connection_->presentation() ||
         frame->pending_feedback || frame->feedback.has_value());
  root_surface->Commit(true);

  frame->root_config = wl::WaylandOverlayConfig();
  frame->subsurfaces_to_overlays.clear();

  // Empty frames do not expect feedbacks so don't push to |submitted_frames_|.
  if (!empty_frame) {
    if (potential_compositor_buffer_lock &&
        ++frames_in_flight_ >= kMaxFramesInFlight) {
      if (freeze_timeout_timer_.IsRunning()) {
        freeze_timeout_timer_.Reset();
      } else {
        freeze_timeout_timer_.Start(FROM_HERE, base::Milliseconds(500), this,
                                    &WaylandFrameManager::FreezeTimeout);
      }
    }
    submitted_frames_.push_back(std::move(frame));
  }

  VerifyNumberOfSubmittedFrames();

  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::DiscardFrame(std::unique_ptr<WaylandFrame> frame) {
  frame->feedback = gfx::PresentationFeedback::Failure();
  submitted_frames_.push_back(std::move(frame));
  VerifyNumberOfSubmittedFrames();
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::ApplySurfaceConfigure(
    WaylandFrame* frame,
    WaylandSurface* surface,
    wl::WaylandOverlayConfig& config,
    bool set_opaque_region) {
  DCHECK(surface);
  if (!config.buffer_id)
    return;

  if (!ValidateRect(config.bounds_rect)) {
    DCHECK(fatal_error_message_.empty());
    // A fatal error must be set here and handled outside the Playback method as
    // terminating the gpu during the playback is illegal - a pending frame will
    // DCHECK in ::ClearStates.
    fatal_error_message_ = kBoundsRectNanOrInf;
    return;
  }

  static const wl_callback_listener frame_listener = {
      &WaylandFrameManager::FrameCallbackDone};
  static const wp_presentation_feedback_listener feedback_listener = {
      &WaylandFrameManager::FeedbackSyncOutput,
      &WaylandFrameManager::FeedbackPresented,
      &WaylandFrameManager::FeedbackDiscarded};

  surface->set_buffer_transform(config.transform);
  surface->set_surface_buffer_scale(config.surface_scale_factor);
  surface->set_buffer_crop(config.crop_rect);
  surface->set_viewport_destination(config.bounds_rect.size());
  surface->set_opacity(config.opacity);
  surface->set_blending(config.enable_blend);
  surface->set_rounded_clip_bounds(
      config.rounded_clip_bounds.value_or(gfx::RRectF()));
  surface->set_overlay_priority(config.priority_hint);
  surface->set_background_color(config.background_color);
  surface->set_contains_video(
      config.priority_hint == gfx::OverlayPriorityHint::kHardwareProtection ||
      config.priority_hint == gfx::OverlayPriorityHint::kVideo);
  surface->set_color_space(
      config.color_space.value_or(gfx::ColorSpace::CreateSRGB()));
  if (set_opaque_region) {
    std::vector<gfx::Rect> region_px = {
        gfx::Rect(gfx::ToRoundedSize(config.bounds_rect.size()))};
    surface->set_opaque_region(config.enable_blend ? nullptr : &region_px);
  }

  WaylandBufferHandle* buffer_handle =
      connection_->buffer_manager_host()->GetBufferHandle(surface,
                                                          config.buffer_id);
  DCHECK(buffer_handle);
  bool will_attach = surface->AttachBuffer(buffer_handle);
  // If we don't attach a released buffer, graphics freeze will occur.
  DCHECK(will_attach || !buffer_handle->released(surface));

  surface->UpdateBufferDamageRegion(config.damage_region);
  if (!config.access_fence_handle.is_null())
    surface->set_acquire_fence(std::move(config.access_fence_handle));

  // If it's a solid color buffer, do not set a release callback as it's not
  // required to wait for this buffer - Wayland compositor only uses that to
  // produce a config for the quad.
  const bool is_solid_color_buffer =
      buffer_handle->backing_type() ==
      WaylandBufferBacking::BufferBackingType::kSolidColor;
  if (will_attach) {
    // Setup frame callback if wayland_surface will commit this buffer.
    // On Mutter, we don't receive frame.callback acks if we don't attach a
    // new wl_buffer, which leads to graphics freeze. So only setup
    // frame_callback when we're attaching a different buffer.
    if (!frame->wl_frame_callback) {
      frame->wl_frame_callback.reset(wl_surface_frame(surface->surface()));
      wl_callback_add_listener(frame->wl_frame_callback.get(), &frame_listener,
                               this);
    }

    if (!is_solid_color_buffer) {
      if (connection_->linux_explicit_synchronization_v1()) {
        surface->RequestExplicitRelease(
            base::BindOnce(&WaylandFrameManager::OnExplicitBufferRelease,
                           weak_factory_.GetWeakPtr(), surface));
      }
      buffer_handle->set_buffer_released_callback(
          base::BindOnce(&WaylandFrameManager::OnWlBufferRelease,
                         weak_factory_.GetWeakPtr(), surface),
          surface);
    }
  }

  if (connection_->presentation() && !frame->pending_feedback) {
    frame->pending_feedback.reset(wp_presentation_feedback(
        connection_->presentation(), surface->surface()));
    wp_presentation_feedback_add_listener(frame->pending_feedback.get(),
                                          &feedback_listener, this);
  }

  if (!is_solid_color_buffer) {
    // If we have submitted this buffer in a previous frame and it is not
    // released yet, submitting the buffer again will not make wayland
    // compositor to release it twice. Remove it from the previous frame.
    for (auto& submitted_frames : submitted_frames_) {
      auto result = submitted_frames->submitted_buffers.find(surface);
      if (result != submitted_frames->submitted_buffers.end() &&
          result->second->wl_buffer() == buffer_handle->wl_buffer()) {
        submitted_frames->submitted_buffers.erase(result);
        break;
      }
    }

    frame->submitted_buffers.emplace(surface, buffer_handle);
  }

  // Send instructions across wayland protocol, but do not commit yet, let the
  // caller decide whether the commit should flush.
  surface->ApplyPendingState();
}

// static
void WaylandFrameManager::FrameCallbackDone(void* data,
                                            struct wl_callback* callback,
                                            uint32_t time) {
  auto* self = static_cast<WaylandFrameManager*>(data);
  DCHECK(self);
  self->OnFrameCallback(callback);
}

void WaylandFrameManager::OnFrameCallback(struct wl_callback* callback) {
  DCHECK(submitted_frames_.back()->wl_frame_callback.get() == callback);
  submitted_frames_.back()->wl_frame_callback.reset();
  MaybeProcessPendingFrame();
}

// static
void WaylandFrameManager::FeedbackSyncOutput(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    struct wl_output* output) {}

// static
void WaylandFrameManager::FeedbackPresented(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec,
    uint32_t refresh,
    uint32_t seq_hi,
    uint32_t seq_lo,
    uint32_t flags) {
  auto* self = static_cast<WaylandFrameManager*>(data);
  DCHECK(self);
  self->OnPresentation(
      wp_presentation_feedback,
      gfx::PresentationFeedback(self->connection_->ConvertPresentationTime(
                                    tv_sec_hi, tv_sec_lo, tv_nsec),
                                base::Nanoseconds(refresh),
                                GetPresentationKindFlags(flags)));
}

// static
void WaylandFrameManager::FeedbackDiscarded(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback) {
  auto* self = static_cast<WaylandFrameManager*>(data);
  DCHECK(self);
  self->OnPresentation(wp_presentation_feedback,
                       gfx::PresentationFeedback::Failure(),
                       true /* discarded */);
}

void WaylandFrameManager::OnPresentation(
    struct wp_presentation_feedback* wp_presentation_feedback,
    const gfx::PresentationFeedback& feedback,
    bool discarded) {
  for (auto& frame : submitted_frames_) {
    if (frame->pending_feedback.get() == wp_presentation_feedback) {
      frame->feedback = feedback;
      break;
    } else if (!frame->feedback.has_value() && !discarded) {
      // Feedback must come in order. However, if one of the feedbacks was
      // discarded and the previous feedbacks haven't been received yet, don't
      // mark previous feedbacks as failed as they will come later. For
      // example, imagine you are waiting for f[0], f[1] and f[2]. f[2] gets
      // discarded, previous ones mustn't be marked as failed as they will
      // come later.
      // TODO(fangzhoug): Exo seems to deliver presentation_feedbacks out of
      // order occasionally, causing us to mark a valid feedback as failed.
      // Investigate the issue with surface sync.
      frame->feedback = gfx::PresentationFeedback::Failure();
    }
    CHECK_NE(frame.get(), submitted_frames_.back().get());
  }
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::VerifyNumberOfSubmittedFrames() {
  // This queue should be small - if not it's likely a bug.
  //
  // Ideally this should be a DCHECK, but if the feedbacks are never sent (a bug
  // in the server, everything crashes).
  if (submitted_frames_.size() >= kMaxNumberOfFrames) {
    LOG(ERROR)
        << "The server has buggy presentation feedback. Discarding all "
           "presentation feedback requests in all frames except the last "
        << kMaxFramesInFlight << ".";
    // Leave three last frames in case if the server restores its behavior
    // (unlikely).
    for (auto it = submitted_frames_.begin();
         it < (submitted_frames_.end() - kMaxFramesInFlight); it++) {
      if (!(*it)->submission_acked || !(*it)->pending_feedback)
        break;
      DCHECK(!(*it)->feedback.has_value());
      (*it)->feedback = gfx::PresentationFeedback::Failure();
      (*it)->pending_feedback.reset();
    }
  }
}

bool WaylandFrameManager::EnsureWlBuffersExist(WaylandFrame& frame) {
  WaylandBufferHandle* handle_pending_creation = nullptr;
  for (auto& subsurface_to_overlay : frame.subsurfaces_to_overlays) {
    if (subsurface_to_overlay.second.buffer_id) {
      auto* handle = connection_->buffer_manager_host()->EnsureBufferHandle(
          subsurface_to_overlay.first->wayland_surface(),
          subsurface_to_overlay.second.buffer_id);
      // Buffer is gone while this frame is pending, remove this config.
      if (!handle) {
        frame.buffer_lost = true;
        subsurface_to_overlay.second = wl::WaylandOverlayConfig();
      } else if (!handle->wl_buffer() && !handle_pending_creation) {
        // Found the first not-ready buffer, let handle invoke
        // MaybeProcessPendingFrame() when wl_buffer is created.
        handle_pending_creation = handle;
      }
    }
  }
  if (frame.root_config.buffer_id) {
    auto* handle = connection_->buffer_manager_host()->EnsureBufferHandle(
        frame.root_surface, frame.root_config.buffer_id);
    if (!handle) {
      frame.buffer_lost = true;
      frame.root_config = wl::WaylandOverlayConfig();
    } else if (!handle->wl_buffer() && !handle_pending_creation) {
      handle_pending_creation = handle;
    }
  }

  // Some buffers might have been lost. No need to wait.
  if (frame.buffer_lost)
    handle_pending_creation = nullptr;

  // There are wl_buffers missing, schedule MaybeProcessPendingFrame so that
  // it's called when buffers are created.
  if (handle_pending_creation) {
    handle_pending_creation->set_buffer_created_callback(
        base::BindOnce(&WaylandFrameManager::MaybeProcessPendingFrame,
                       weak_factory_.GetWeakPtr()));
  }
  return !!handle_pending_creation;
}

void WaylandFrameManager::OnExplicitBufferRelease(WaylandSurface* surface,
                                                  struct wl_buffer* wl_buffer,
                                                  base::ScopedFD fence) {
  DCHECK(wl_buffer);

  // Releases may not necessarily come in order, so search the submitted
  // buffers.
  for (const auto& frame : submitted_frames_) {
    auto result = frame->submitted_buffers.find(surface);
    if (result != frame->submitted_buffers.end() &&
        result->second->wl_buffer() == wl_buffer) {
      // Explicitly make this buffer released when
      // linux_explicit_synchronization is used.
      result->second->OnExplicitRelease(surface);

      if (fence.is_valid()) {
        if (frame->merged_release_fence_fd.is_valid()) {
          frame->merged_release_fence_fd.reset(sync_merge(
              "", frame->merged_release_fence_fd.get(), fence.get()));
        } else {
          frame->merged_release_fence_fd = std::move(fence);
        }
        DCHECK(frame->merged_release_fence_fd.is_valid());
      }

      frame->submitted_buffers.erase(result);
      break;
    }
  }

  // A release means we may be able to send OnSubmission for previously
  // submitted buffers.
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::OnWlBufferRelease(WaylandSurface* surface,
                                            struct wl_buffer* wl_buffer) {
  DCHECK(wl_buffer);

  // Releases may not necessarily come in order, so search the submitted
  // buffers.
  for (const auto& frame : submitted_frames_) {
    auto result = frame->submitted_buffers.find(surface);
    if (result != frame->submitted_buffers.end() &&
        result->second->wl_buffer() == wl_buffer) {
      frame->submitted_buffers.erase(result);
      break;
    }
  }

  // A release means we may be able to send OnSubmission for previously
  // submitted buffers.
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::MaybeProcessSubmittedFrames() {
  if (submitted_frames_.empty())
    return;

  // Determine the range of frames in `submitted_frames_` that we want to send
  // OnSubmission. The range is specified by
  //   [`on_submission_begin`, `on_submission_begin` + `on_submission_count`).
  base::circular_deque<std::unique_ptr<WaylandFrame>>::iterator
      on_submission_begin;
  int32_t on_submission_count = 0;

  // We force an OnSubmission call for the very first buffer submitted,
  // otherwise buffers are not acked in a quiescent state. We keep track of
  // whether it has already been acked. A buffer may have already been acked
  // if it is the first buffer submitted and it is destroyed before being
  // explicitly released. In that case, don't send an OnSubmission.
  if (submitted_frames_.size() == 1u &&
      !submitted_frames_.front()->submission_acked) {
    ProcessOldSubmittedFrame(submitted_frames_.front().get());
    on_submission_begin = submitted_frames_.begin();
    on_submission_count = 1;
  }

  // Buffers may be released out of order, but we need to provide the
  // guarantee that OnSubmission will be called in order of buffer submission.
  for (auto iter = submitted_frames_.begin();
       iter < submitted_frames_.end() - 1; ++iter) {
    // Treat a buffer as released if it has been explicitly released or
    // destroyed.
    bool all_buffers_released = (*iter)->submitted_buffers.empty();
    // We can send OnSubmission for the |iter + 1| if the buffers of |iter| are
    // all released.
    if (!all_buffers_released)
      break;

    DCHECK((*iter)->submission_acked);
    if ((*(iter + 1))->submission_acked)
      continue;

    // Prepare to call OnSubmission() for |iter + 1| since |iter| is fully
    // released.
    // We had to wait for this release because SwapCompletionCallback
    // indicates to the client that the buffers in previous frame is available
    // for reuse.
    ProcessOldSubmittedFrame((iter + 1)->get());
    if (on_submission_count == 0) {
      on_submission_begin = iter + 1;
    }
    on_submission_count++;
  }

  if (on_submission_count > 0) {
    std::vector<wl::WaylandPresentationInfo> presentation_infos =
        GetReadyPresentations();

    for (int32_t i = 0; i < on_submission_count; ++i) {
      auto iter = on_submission_begin + i;

      gfx::GpuFenceHandle release_fence_handle;
      if (iter != submitted_frames_.begin()) {
        auto prev_iter = iter - 1;
        if ((*prev_iter)->merged_release_fence_fd.is_valid()) {
          release_fence_handle.owned_fd =
              std::move((*prev_iter)->merged_release_fence_fd);
        }
      }

      // The presentation info entries are sent with the last OnSubmission()
      // call.
      connection_->buffer_manager_host()->OnSubmission(
          window_->GetWidget(), (*iter)->frame_id, gfx::SwapResult::SWAP_ACK,
          std::move(release_fence_handle),
          (i != on_submission_count - 1)
              ? std::vector<wl::WaylandPresentationInfo>()
              : presentation_infos);
    }
  }

  ClearProcessedSubmittedFrames();
  DCHECK_LE(submitted_frames_.size(), kMaxNumberOfFrames);

  UpdatePresentationFlushTimer();
}

void WaylandFrameManager::ProcessOldSubmittedFrame(WaylandFrame* frame) {
  DCHECK(!submitted_frames_.empty());
  DCHECK(!frame->submission_acked);
  DCHECK(!frame->presentation_acked);
  DCHECK(frame->pending_feedback || frame->feedback.has_value() ||
         !connection_->presentation());
  frame->submission_acked = true;

  if (potential_compositor_buffer_lock &&
      frame != submitted_frames_.front().get()) {
    --frames_in_flight_;
    freeze_timeout_timer_.Stop();
  }

  // If presentation feedback is not supported, use a fake feedback. This
  // literally means there are no presentation feedback callbacks created.
  if (!connection_->presentation()) {
    DCHECK(!frame->feedback.has_value() || frame->feedback->failed());
    frame->feedback = frame->feedback.value_or(
        gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(),
                                  GetPresentationKindFlags(0)));
  }
}

std::vector<wl::WaylandPresentationInfo>
WaylandFrameManager::GetReadyPresentations() {
  std::vector<wl::WaylandPresentationInfo> results;
  for (auto& frame : submitted_frames_) {
    if (!frame->submission_acked || !frame->feedback.has_value()) {
      break;
    }
    if (frame->presentation_acked) {
      continue;
    }
    frame->presentation_acked = true;
    results.emplace_back(frame->frame_id, frame->feedback.value());
  }

  return results;
}

bool WaylandFrameManager::HaveReadyPresentations() const {
  for (auto& frame : submitted_frames_) {
    if (!frame->submission_acked || !frame->feedback.has_value()) {
      break;
    }
    if (frame->presentation_acked) {
      continue;
    }
    return true;
  }
  return false;
}

void WaylandFrameManager::ClearProcessedSubmittedFrames() {
  while (submitted_frames_.size() > 1 &&
         submitted_frames_.front()->submitted_buffers.empty() &&
         submitted_frames_.front()->presentation_acked) {
    DCHECK(submitted_frames_.front()->submission_acked);
    submitted_frames_.pop_front();
  }
}

void WaylandFrameManager::FreezeTimeout() {
  LOG(WARNING) << "Freeze detected, immediately release a frame";
  for (auto& frame : submitted_frames_) {
    if (frame->submitted_buffers.empty())
      continue;
    frame->submitted_buffers.clear();
    MaybeProcessSubmittedFrames();
    return;
  }
  NOTREACHED();
}

void WaylandFrameManager::Hide() {
  if (!submitted_frames_.empty() &&
      submitted_frames_.back()->wl_frame_callback) {
    submitted_frames_.back()->wl_frame_callback.reset();
    // Mutter sometimes does not call buffer.release if wl_surface role is
    // destroyed, causing graphics freeze. Manually release them and trigger
    // OnSubmission callbacks.
    for (auto& submitted : submitted_frames_.back()->submitted_buffers)
      submitted.second->OnExplicitRelease(submitted.first);
    submitted_frames_.back()->submitted_buffers.clear();
    submitted_frames_.back()->feedback = gfx::PresentationFeedback::Failure();
  }

  // Discard |pending_frames_| since they're not going to be used when hidden.
  for (auto& frame : pending_frames_) {
    DCHECK(frame) << "Can't perform Hide() during a frame playback.";
    frame->feedback = gfx::PresentationFeedback::Failure();
    submitted_frames_.push_back(std::move(frame));
  }
  pending_frames_.clear();

  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::ClearStates() {
  // Clear the previous fatal error message as it might have been set during
  // a playback.
  fatal_error_message_.clear();

  for (auto& frame : submitted_frames_) {
    for (auto& submitted : frame->submitted_buffers)
      submitted.second->OnExplicitRelease(submitted.first);
  }
  submitted_frames_.clear();

  for (auto& frame : pending_frames_) {
    DCHECK(frame)
        << "Can't perform OnChannelDestroyed() during a frame playback.";
  }
  pending_frames_.clear();

  presentation_flush_timer_.Stop();
}

// static
base::TimeDelta
WaylandFrameManager::GetPresentationFlushTimerDurationForTesting() {
  return kPresentationFlushTimerDuration;
}

void WaylandFrameManager::UpdatePresentationFlushTimer() {
  if (HaveReadyPresentations()) {
    if (!presentation_flush_timer_.IsRunning()) {
      presentation_flush_timer_.Start(
          FROM_HERE, kPresentationFlushTimerDuration, this,
          &WaylandFrameManager::OnPresentationFlushTimerFired);
    }

    return;
  }

  if (presentation_flush_timer_.IsRunning()) {
    // There is no queued presentation. Decide whether to stop the presentation
    // flush timer.
    //
    // If we unconditionally stop the timer here, it is logically correct, but
    // often results in frequent timer starts and stops. Imagine we have
    // interleaved submissions and presentations:
    //   submission_1 - presentation_1 - submission_2 - presetnation_2 - ...
    // Then we will always start timer when we get presentation_i, and stop
    // timer when we get submission_(i+1), at which point we send an
    // OnSubmission IPC carrying both submission_(i+1) and presentation_i.
    //
    // In order to reduce timer starts/stops, here we choose not to stop the
    // timer, except for one case: when it gets close enough to the target time
    // of the timer. The reason is that if we don't stop the timer in this case,
    // it is likely to fire before the next submission, resulting in either
    //   (1) a no-op (if no presentation arrives before timer firing); or
    //   (2) an extra OnPresentation IPC (if a presentation arrives before timer
    //   firing), which could have been piggybacked by the next submission. This
    //   is an expensive case that we want to avoid.
    const base::TimeDelta remaining_delay =
        presentation_flush_timer_.desired_run_time() - base::TimeTicks::Now();
    if (remaining_delay <= kPresentationFlushTimerStopThreshold) {
      presentation_flush_timer_.Stop();
    }
  }
}

void WaylandFrameManager::OnPresentationFlushTimerFired() {
  std::vector<wl::WaylandPresentationInfo> presentation_infos =
      GetReadyPresentations();
  if (presentation_infos.empty()) {
    return;
  }
  connection_->buffer_manager_host()->OnPresentation(window_->GetWidget(),
                                                     presentation_infos);

  ClearProcessedSubmittedFrames();
}

}  // namespace ui
