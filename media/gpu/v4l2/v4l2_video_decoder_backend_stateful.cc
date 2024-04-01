// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateful.h"
#include <cstddef>

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_stateful_workaround.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

V4L2StatefulVideoDecoderBackend::DecodeRequest::DecodeRequest(
    scoped_refptr<DecoderBuffer> buf,
    VideoDecoder::DecodeCB cb,
    int32_t id)
    : buffer(std::move(buf)), decode_cb(std::move(cb)), bitstream_id(id) {}

V4L2StatefulVideoDecoderBackend::DecodeRequest::DecodeRequest(DecodeRequest&&) =
    default;
V4L2StatefulVideoDecoderBackend::DecodeRequest&
V4L2StatefulVideoDecoderBackend::DecodeRequest::operator=(DecodeRequest&&) =
    default;

V4L2StatefulVideoDecoderBackend::DecodeRequest::~DecodeRequest() = default;

bool V4L2StatefulVideoDecoderBackend::DecodeRequest::IsCompleted() const {
  return bytes_used == buffer->data_size();
}

V4L2StatefulVideoDecoderBackend::V4L2StatefulVideoDecoderBackend(
    Client* const client,
    scoped_refptr<V4L2Device> device,
    VideoCodecProfile profile,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : V4L2VideoDecoderBackend(client, std::move(device)),
      profile_(profile),
      task_runner_(task_runner) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2StatefulVideoDecoderBackend::~V4L2StatefulVideoDecoderBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (flush_cb_ || current_decode_request_ || !decode_request_queue_.empty()) {
    VLOGF(1) << "Should not destroy backend during pending decode!";
  }

  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  if (device_->Ioctl(VIDIOC_UNSUBSCRIBE_EVENT, &sub) != 0) {
    VLOGF(1) << "Cannot unsubscribe to event";
  }
}

bool V4L2StatefulVideoDecoderBackend::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (!IsSupportedProfile(profile_)) {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile_);
    return false;
  }

  frame_splitter_ =
      v4l2_vda_helpers::InputBufferFragmentSplitter::CreateFromProfile(
          profile_);
  if (!frame_splitter_) {
    VLOGF(1) << "Failed to create frame splitter";
    return false;
  }

  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  if (device_->Ioctl(VIDIOC_SUBSCRIBE_EVENT, &sub) != 0) {
    VLOGF(1) << "Cannot subscribe to event";
    return false;
  }

  framerate_control_ =
      std::make_unique<V4L2FrameRateControl>(device_, task_runner_);

  return true;
}

void V4L2StatefulVideoDecoderBackend::EnqueueDecodeTask(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_cb,
    int32_t bitstream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (!buffer->end_of_stream()) {
    has_pending_requests_ = true;
  }

  decode_request_queue_.push(
      DecodeRequest(std::move(buffer), std::move(decode_cb), bitstream_id));

  DoDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::DoDecodeWork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Do not decode if a flush is in progress.
  // This may actually be ok to do if we are changing resolution?
  if (flush_cb_)
    return;

  // Get a new decode request if none is in progress.
  if (!current_decode_request_) {
    // No more decode request, nothing to do for now.
    if (decode_request_queue_.empty())
      return;

    auto decode_request = std::move(decode_request_queue_.front());
    decode_request_queue_.pop();

    // Need to flush?
    if (decode_request.buffer->end_of_stream()) {
      InitiateFlush(std::move(decode_request.decode_cb));
      return;
    }

    // This is our new decode request.
    current_decode_request_ = std::move(decode_request);
    DCHECK_EQ(current_decode_request_->bytes_used, 0u);

    if (VideoCodecProfileToVideoCodec(profile_) == VideoCodec::kVP9 &&
        !AppendVP9SuperFrameIndexIfNeeded(current_decode_request_->buffer)) {
      VLOGF(1) << "Failed to append superframe index for VP9 k-SVC frame";
    }
  }

  // Get a V4L2 buffer to copy the encoded data into.
  if (!current_input_buffer_) {
    current_input_buffer_ = input_queue_->GetFreeBuffer();
    // We will be called again once an input buffer becomes available.
    if (!current_input_buffer_)
      return;

    // Record timestamp of the input buffer so it propagates to the decoded
    // frames.
    const struct timespec timespec =
        current_decode_request_->buffer->timestamp().ToTimeSpec();
    struct timeval timestamp = {
        .tv_sec = timespec.tv_sec,
        .tv_usec = timespec.tv_nsec / 1000,
    };
    current_input_buffer_->SetTimeStamp(timestamp);

    const int64_t flat_timespec =
        base::TimeDelta::FromTimeSpec(timespec).InMilliseconds();
    encoding_timestamps_[flat_timespec] = base::TimeTicks::Now();
  }

  // From here on we have both a decode request and input buffer, so we can
  // progress with decoding.
  DCHECK(current_decode_request_.has_value());
  DCHECK(current_input_buffer_.has_value());

  const DecoderBuffer* current_buffer = current_decode_request_->buffer.get();
  DCHECK_LT(current_decode_request_->bytes_used, current_buffer->data_size());
  const uint8_t* const data =
      current_buffer->data() + current_decode_request_->bytes_used;
  const size_t data_size =
      current_buffer->data_size() - current_decode_request_->bytes_used;
  size_t bytes_to_copy = 0;

  if (!frame_splitter_->AdvanceFrameFragment(data, data_size, &bytes_to_copy)) {
    VLOGF(1) << "Invalid H.264 stream detected.";
    std::move(current_decode_request_->decode_cb)
        .Run(DecodeStatus::DECODE_ERROR);
    current_decode_request_.reset();
    current_input_buffer_.reset();
    client_->OnBackendError();
    return;
  }

  const size_t bytes_used = current_input_buffer_->GetPlaneBytesUsed(0);
  if (bytes_used + bytes_to_copy > current_input_buffer_->GetPlaneSize(0)) {
    VLOGF(1) << "V4L2 buffer size is too small to contain a whole frame.";
    std::move(current_decode_request_->decode_cb)
        .Run(DecodeStatus::DECODE_ERROR);
    current_decode_request_.reset();
    current_input_buffer_.reset();
    client_->OnBackendError();
    return;
  }

  uint8_t* dst =
      static_cast<uint8_t*>(current_input_buffer_->GetPlaneMapping(0)) +
      bytes_used;
  memcpy(dst, data, bytes_to_copy);
  current_input_buffer_->SetPlaneBytesUsed(0, bytes_used + bytes_to_copy);
  current_decode_request_->bytes_used += bytes_to_copy;

  // Release current_input_request_ if we reached its end.
  if (current_decode_request_->IsCompleted()) {
    std::move(current_decode_request_->decode_cb).Run(DecodeStatus::OK);
    current_decode_request_.reset();
  }

  // If we have a partial frame, wait before submitting it.
  if (frame_splitter_->IsPartialFramePending()) {
    VLOGF(4) << "Partial frame pending, not queueing any buffer now.";
    return;
  }

  // The V4L2 input buffer contains a decodable entity, queue it.
  if (!std::move(*current_input_buffer_).QueueMMap()) {
    LOG(ERROR) << "Error while queuing input buffer!";
    client_->OnBackendError();
  }
  current_input_buffer_.reset();

  // If we can still progress on a decode request, do it.
  if (current_decode_request_ || !decode_request_queue_.empty())
    ScheduleDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::ScheduleDecodeWork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2StatefulVideoDecoderBackend::DoDecodeWork,
                                weak_this_));
}

void V4L2StatefulVideoDecoderBackend::ProcessEventQueue() {
  while (absl::optional<struct v4l2_event> ev = device_->DequeueEvent()) {
    if (ev->type == V4L2_EVENT_SOURCE_CHANGE &&
        (ev->u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION)) {
      ChangeResolution();
    }
  }
}

void V4L2StatefulVideoDecoderBackend::OnServiceDeviceTask(bool event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (event)
    ProcessEventQueue();

  // We can enqueue dequeued output buffers immediately.
  EnqueueOutputBuffers();

  // Try to progress on our work since we may have dequeued input buffers.
  DoDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::EnqueueOutputBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  const v4l2_memory mem_type = output_queue_->GetMemoryType();

  while (true) {
    bool ret = false;
    bool no_buffer = false;

    absl::optional<V4L2WritableBufferRef> buffer;
    switch (mem_type) {
      case V4L2_MEMORY_MMAP:
        buffer = output_queue_->GetFreeBuffer();
        if (!buffer) {
          no_buffer = true;
          break;
        }

        ret = std::move(*buffer).QueueMMap();
        break;
      case V4L2_MEMORY_DMABUF: {
        scoped_refptr<VideoFrame> video_frame = GetPoolVideoFrame();
        // Running out of frame is not an error, we will be called again
        // once frames are available.
        if (!video_frame)
          return;
        buffer = output_queue_->GetFreeBufferForFrame(*video_frame);
        if (!buffer) {
          no_buffer = true;
          break;
        }

        framerate_control_->AttachToVideoFrame(video_frame);
        ret = std::move(*buffer).QueueDMABuf(std::move(video_frame));
        break;
      }
      default:
        NOTREACHED();
    }

    // Running out of V4L2 buffers is not an error, so just exit the loop
    // gracefully.
    if (no_buffer)
      break;

    if (!ret) {
      LOG(ERROR) << "Error while queueing output buffer!";
      client_->OnBackendError();
    }
  }

  DVLOGF(3) << output_queue_->QueuedBuffersCount() << "/"
            << output_queue_->AllocatedBuffersCount()
            << " output buffers queued";
}

scoped_refptr<VideoFrame> V4L2StatefulVideoDecoderBackend::GetPoolVideoFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  DmabufVideoFramePool* pool = client_->GetVideoFramePool();
  DCHECK_EQ(output_queue_->GetMemoryType(), V4L2_MEMORY_DMABUF);
  DCHECK_NE(pool, nullptr);

  scoped_refptr<VideoFrame> frame = pool->GetFrame();
  if (!frame) {
    DVLOGF(3) << "No available videoframe for now";
    // We will try again once a frame becomes available.
    pool->NotifyWhenFrameAvailable(base::BindOnce(
        base::IgnoreResult(&base::SequencedTaskRunner::PostTask), task_runner_,
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(
                &V4L2StatefulVideoDecoderBackend::EnqueueOutputBuffers),
            weak_this_)));
  }

  return frame;
}

// static
void V4L2StatefulVideoDecoderBackend::ReuseOutputBufferThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    absl::optional<base::WeakPtr<V4L2StatefulVideoDecoderBackend>> weak_this,
    V4L2ReadableBufferRef buffer) {
  DVLOGF(3);
  DCHECK(weak_this);

  if (task_runner->RunsTasksInCurrentSequence()) {
    if (*weak_this)
      (*weak_this)->ReuseOutputBuffer(std::move(buffer));
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatefulVideoDecoderBackend::ReuseOutputBuffer,
                       *weak_this, std::move(buffer)));
  }
}

void V4L2StatefulVideoDecoderBackend::ReuseOutputBuffer(
    V4L2ReadableBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << "Reuse output buffer #" << buffer->BufferId();

  // Lose reference to the buffer so it goes back to the free list.
  buffer.reset();

  // Enqueue the newly available buffer.
  EnqueueOutputBuffers();
}

void V4L2StatefulVideoDecoderBackend::OnOutputBufferDequeued(
    V4L2ReadableBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Zero-bytes buffers are returned as part of a flush and can be dismissed.
  if (buffer->GetPlaneBytesUsed(0) > 0) {
    const struct timeval timeval = buffer->GetTimeStamp();
    const struct timespec timespec = {
        .tv_sec = timeval.tv_sec,
        .tv_nsec = timeval.tv_usec * 1000,
    };

    const int64_t flat_timespec =
        base::TimeDelta::FromTimeSpec(timespec).InMilliseconds();
    // TODO(b/190615065) |flat_timespec| might be repeated with H.264
    // bitstreams, investigate why, and change the if() to DCHECK().
    if (base::Contains(encoding_timestamps_, flat_timespec)) {
      UMA_HISTOGRAM_TIMES(
          "Media.PlatformVideoDecoding.Decode",
          base::TimeTicks::Now() - encoding_timestamps_[flat_timespec]);
      encoding_timestamps_.erase(flat_timespec);
    }

    scoped_refptr<VideoFrame> frame;

    switch (output_queue_->GetMemoryType()) {
      case V4L2_MEMORY_MMAP: {
        // Wrap the videoframe into another one so we can be signaled when the
        // consumer is done with it and reuse the V4L2 buffer.
        scoped_refptr<VideoFrame> origin_frame = buffer->GetVideoFrame();
        frame = VideoFrame::WrapVideoFrame(origin_frame, origin_frame->format(),
                                           origin_frame->visible_rect(),
                                           origin_frame->natural_size());
        frame->AddDestructionObserver(base::BindOnce(
            &V4L2StatefulVideoDecoderBackend::ReuseOutputBufferThunk,
            task_runner_, weak_this_, buffer));
        break;
      }
      case V4L2_MEMORY_DMABUF:
        // The pool VideoFrame we passed to QueueDMABuf() has been decoded into,
        // pass it as-is.
        frame = buffer->GetVideoFrame();
        break;
      default:
        NOTREACHED();
    }

    const base::TimeDelta timestamp = base::TimeDelta::FromTimeSpec(timespec);
    client_->OutputFrame(std::move(frame), *visible_rect_, timestamp);
  }

  // We were waiting for the last buffer before a resolution change
  // The order here is important! A flush event may come after a resolution
  // change event (but not the opposite), so we must make sure both events
  // are processed in the correct order.
  if (buffer->IsLast()){
    // Check that we don't have a resolution change event pending. If we do
    // then this LAST buffer was related to it.
    ProcessEventQueue();

    if (resolution_change_cb_) {
      std::move(resolution_change_cb_).Run();
    } else if (flush_cb_) {
      // We were waiting for a flush to complete, and received the last buffer.
      CompleteFlush();
    }
  }

  EnqueueOutputBuffers();
}

bool V4L2StatefulVideoDecoderBackend::SendStopCommand() {
  struct v4l2_decoder_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.cmd = V4L2_DEC_CMD_STOP;
  if (device_->Ioctl(VIDIOC_DECODER_CMD, &cmd) != 0) {
    LOG(ERROR) << "Failed to issue STOP command";
    client_->OnBackendError();
    return false;
  }

  return true;
}

bool V4L2StatefulVideoDecoderBackend::InitiateFlush(
    VideoDecoder::DecodeCB flush_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  DCHECK(!flush_cb_);

  // Submit any pending input buffer at the time of flush.
  if (current_input_buffer_) {
    if (!std::move(*current_input_buffer_).QueueMMap()) {
      LOG(ERROR) << "Error while queuing input buffer!";
      client_->OnBackendError();
    }
    current_input_buffer_.reset();
  }

  client_->InitiateFlush();
  flush_cb_ = std::move(flush_cb);

  // Special case: if we haven't received any decoding request, we could
  // complete the flush immediately.
  if (!has_pending_requests_)
    return CompleteFlush();

  if (output_queue_->IsStreaming()) {
    // If the CAPTURE queue is streaming, send the STOP command to the V4L2
    // device. The device will let us know that the flush is completed by
    // sending us a CAPTURE buffer with the LAST flag set.
    return SendStopCommand();
  } else {
    // If the CAPTURE queue is not streaming, this means we received the flush
    // request before the initial resolution has been established. The flush
    // request will be processed in OnChangeResolutionDone(), when the CAPTURE
    // queue starts streaming.
    DVLOGF(2) << "Flush request to be processed after CAPTURE queue starts";
  }

  return true;
}

bool V4L2StatefulVideoDecoderBackend::CompleteFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  DCHECK(flush_cb_);

  // Signal that flush has properly been completed.
  std::move(flush_cb_).Run(DecodeStatus::OK);

  // If CAPTURE queue is streaming, send the START command to the V4L2 device
  // to signal that we are resuming decoding with the same state.
  if (output_queue_->IsStreaming()) {
    struct v4l2_decoder_cmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = V4L2_DEC_CMD_START;
    if (device_->Ioctl(VIDIOC_DECODER_CMD, &cmd) != 0) {
      LOG(ERROR) << "Failed to issue START command";
      std::move(flush_cb_).Run(DecodeStatus::DECODE_ERROR);
      client_->OnBackendError();
      return false;
    }
  }

  client_->CompleteFlush();

  // Resume decoding if data is available.
  ScheduleDecodeWork();

  has_pending_requests_ = false;
  return true;
}

void V4L2StatefulVideoDecoderBackend::OnStreamStopped(bool stop_input_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // If we are resetting, also reset the splitter.
  if (frame_splitter_ && stop_input_queue)
    frame_splitter_->Reset();
}

void V4L2StatefulVideoDecoderBackend::ChangeResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Here we just query the new resolution, visible rect, and number of output
  // buffers before asking the client to update the resolution.

  auto format = output_queue_->GetFormat().first;
  if (!format) {
    client_->OnBackendError();
    return;
  }
  const gfx::Size pic_size(format->fmt.pix_mp.width, format->fmt.pix_mp.height);

  auto visible_rect = output_queue_->GetVisibleRect();
  if (!visible_rect) {
    client_->OnBackendError();
    return;
  }

  if (!gfx::Rect(pic_size).Contains(*visible_rect)) {
    client_->OnBackendError();
    return;
  }

  auto ctrl = device_->GetCtrl(V4L2_CID_MIN_BUFFERS_FOR_CAPTURE);
  constexpr size_t DEFAULT_NUM_OUTPUT_BUFFERS = 7;
  const size_t num_output_buffers =
      ctrl ? ctrl->value : DEFAULT_NUM_OUTPUT_BUFFERS;
  if (!ctrl)
    VLOGF(1) << "Using default minimum number of CAPTURE buffers";

  // Signal that we are flushing and initiate the resolution change.
  // Our flush will be done when we receive a buffer with the LAST flag on the
  // CAPTURE queue.
  client_->InitiateFlush();
  DCHECK(!resolution_change_cb_);
  resolution_change_cb_ =
      base::BindOnce(&V4L2StatefulVideoDecoderBackend::ContinueChangeResolution,
                     weak_this_, pic_size, *visible_rect, num_output_buffers);

  // ...that is, unless we are not streaming yet, in which case the resolution
  // change can take place immediately.
  if (!output_queue_->IsStreaming())
    std::move(resolution_change_cb_).Run();
}

void V4L2StatefulVideoDecoderBackend::ContinueChangeResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect,
    const size_t num_output_buffers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Flush is done, but stay in flushing state and ask our client to set the new
  // resolution.
  client_->ChangeResolution(pic_size, visible_rect, num_output_buffers);
}

bool V4L2StatefulVideoDecoderBackend::ApplyResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect,
    const size_t num_output_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Use the visible rect for all new frames.
  visible_rect_ = visible_rect;

  return true;
}

void V4L2StatefulVideoDecoderBackend::OnChangeResolutionDone(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (!success) {
    client_->OnBackendError();
    return;
  }

  // Flush can be considered completed on the client side.
  client_->CompleteFlush();

  // Enqueue all available output buffers now that they are allocated.
  EnqueueOutputBuffers();

  // If we had a flush request pending before the initial resolution change,
  // process it now.
  if (flush_cb_) {
    DVLOGF(2) << "Processing pending flush request...";

    client_->InitiateFlush();
    if (!SendStopCommand())
      return;
  }

  // Also try to progress on our work.
  DoDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::ClearPendingRequests(
    DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (resolution_change_cb_)
    std::move(resolution_change_cb_).Run();

  if (flush_cb_) {
    std::move(flush_cb_).Run(status);
  }

  current_input_buffer_.reset();

  if (current_decode_request_) {
    std::move(current_decode_request_->decode_cb).Run(status);
    current_decode_request_.reset();
  }

  while (!decode_request_queue_.empty()) {
    std::move(decode_request_queue_.front().decode_cb).Run(status);
    decode_request_queue_.pop();
  }

  has_pending_requests_ = false;
}

// TODO(b:149663704) move into helper function shared between both backends?
bool V4L2StatefulVideoDecoderBackend::IsSupportedProfile(
    VideoCodecProfile profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_);
  if (supported_profiles_.empty()) {
    constexpr uint32_t kSupportedInputFourccs[] = {
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_VP8,
        V4L2_PIX_FMT_VP9,
    };
    scoped_refptr<V4L2Device> device = V4L2Device::Create();
    VideoDecodeAccelerator::SupportedProfiles profiles =
        device->GetSupportedDecodeProfiles(base::size(kSupportedInputFourccs),
                                           kSupportedInputFourccs);
    for (const auto& profile : profiles)
      supported_profiles_.push_back(profile.profile);
  }
  return std::find(supported_profiles_.begin(), supported_profiles_.end(),
                   profile) != supported_profiles_.end();
}

bool V4L2StatefulVideoDecoderBackend::StopInputQueueOnResChange() const {
  return false;
}

}  // namespace media
