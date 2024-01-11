// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/raspi/shared/open_max/video_decoder.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

using std::placeholders::_1;

const size_t kResourcePoolSize = 26;
// TODO: Make this configurable inside SbPlayerCreate().
const int64_t kUpdateIntervalUsec = 5'000;

}  // namespace

VideoDecoder::VideoDecoder(SbMediaVideoCodec video_codec)
    : resource_pool_(new DispmanxResourcePool(kResourcePoolSize)),
      eos_written_(false),
      thread_(kSbThreadInvalid),
      request_thread_termination_(false) {
  SB_DCHECK(video_codec == kSbMediaVideoCodecH264);
  update_job_ = std::bind(&VideoDecoder::Update, this);
  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);
}

VideoDecoder::~VideoDecoder() {
  if (SbThreadIsValid(thread_)) {
    {
      ScopedLock scoped_lock(mutex_);
      request_thread_termination_ = true;
    }
    SbThreadJoin(thread_, NULL);
  }
  RemoveJobByToken(update_job_token_);
}

void VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;

  SB_DCHECK(!SbThreadIsValid(thread_));
  thread_ = SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                           "omx_video_decoder", &VideoDecoder::ThreadEntryPoint,
                           this);
  SB_DCHECK(SbThreadIsValid(thread_));
}

void VideoDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(!eos_written_);

  first_input_written_ = true;
  const auto& input_buffer = input_buffers[0];
  queue_.Put(new Event(input_buffer));
  if (!TryToDeliverOneFrame()) {
    SbThreadSleep(1000);
    // Call the callback with NULL frame to ensure that the host knows that
    // more data is expected.
    decoder_status_cb_(kNeedMoreInput, NULL);
  }
}

void VideoDecoder::WriteEndOfStream() {
  eos_written_ = true;
  if (first_input_written_) {
    queue_.Put(new Event(Event::kWriteEOS));
  } else {
    decoder_status_cb_(kNeedMoreInput, VideoFrame::CreateEOSFrame());
  }
}

void VideoDecoder::Reset() {
  queue_.Put(new Event(Event::kReset));
  // TODO: we should introduce a wait here for |queue_| to be fully processed
  // before returning however the wait time is very long (over 40 seconds).
  // The cause for the long wait is unknown and should be investigated.
  eos_written_ = false;
  first_input_written_ = false;
}

void VideoDecoder::Update() {
  if (eos_written_) {
    TryToDeliverOneFrame();
  }
  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);
}

bool VideoDecoder::TryToDeliverOneFrame() {
  scoped_refptr<VideoFrame> frame;
  {
    ScopedLock scoped_lock(mutex_);
    if (filled_buffers_.empty()) {
      return false;
    }
    OMX_BUFFERHEADERTYPE* buffer = filled_buffers_.front();
    frame = CreateFrame(buffer);
    if (!frame) {
      return false;
    }

    SB_DCHECK(!filled_buffers_.empty());
    filled_buffers_.pop();
    freed_buffers_.push(buffer);
  }
  decoder_status_cb_(kNeedMoreInput, frame);
  return true;
}

// static
void* VideoDecoder::ThreadEntryPoint(void* context) {
  VideoDecoder* decoder = reinterpret_cast<VideoDecoder*>(context);
  decoder->RunLoop();
  return NULL;
}

void VideoDecoder::RunLoop() {
  bool stream_ended = false;
  bool eos_written = false;
  OpenMaxVideoDecodeComponent component;

  component.Start();

  scoped_refptr<InputBuffer> current_buffer;
  int offset = 0;

  for (;;) {
    OMX_BUFFERHEADERTYPE* buffer = NULL;
    {
      ScopedLock scoped_lock(mutex_);

      if (request_thread_termination_) {
        break;
      }
      if (!freed_buffers_.empty()) {
        buffer = freed_buffers_.front();
        freed_buffers_.pop();
      }
    }
    if (buffer != NULL) {
      component.DropOutputBuffer(buffer);
    }

    if (OMX_BUFFERHEADERTYPE* buffer = component.GetOutputBuffer()) {
      ScopedLock scoped_lock(mutex_);
      filled_buffers_.push(buffer);
    }

    if (current_buffer) {
      int size = static_cast<int>(current_buffer->size());
      while (offset < size) {
        int written = component.WriteData(
            current_buffer->data() + offset, size - offset,
            OpenMaxComponent::kDataNonEOS, current_buffer->timestamp());
        SB_DCHECK(written >= 0);
        offset += written;
        if (written == 0) {
          break;
        }
      }
      if (offset == size) {
        current_buffer = NULL;
        offset = 0;
      } else {
        SbThreadSleep(1000);
        continue;
      }
    }

    if (stream_ended && !eos_written) {
      eos_written = component.WriteEOS();
    }

    Event* event = queue_.GetTimed(1000);
    if (event == NULL) {
      continue;
    }

    if (event->type == Event::kWriteInputBuffer) {
      if (stream_ended) {
        SB_LOG(ERROR)
            << "WriteInputFrame() was called after WriteEndOfStream().";
      } else {
        offset = 0;
        current_buffer = event->input_buffer;
      }
    } else if (event->type == Event::kWriteEOS) {
      SB_DCHECK(!stream_ended);
      eos_written = component.WriteEOS();
      stream_ended = true;
    } else if (event->type == Event::kReset) {
      ScopedLock scoped_lock(mutex_);

      while (!freed_buffers_.empty()) {
        component.DropOutputBuffer(freed_buffers_.front());
        freed_buffers_.pop();
      }

      while (!filled_buffers_.empty()) {
        component.DropOutputBuffer(filled_buffers_.front());
        filled_buffers_.pop();
      }

      component.Flush();
      stream_ended = false;
      eos_written = false;
    } else {
      SB_NOTREACHED() << "event type " << event->type;
    }
    delete event;
  }

  while (Event* event = queue_.GetTimed(0)) {
    delete event;
  }

  ScopedLock scoped_lock(mutex_);
  while (!freed_buffers_.empty()) {
    component.DropOutputBuffer(freed_buffers_.front());
    freed_buffers_.pop();
  }

  while (!filled_buffers_.empty()) {
    component.DropOutputBuffer(filled_buffers_.front());
    filled_buffers_.pop();
  }
}

scoped_refptr<VideoDecoder::VideoFrame> VideoDecoder::CreateFrame(
    const OMX_BUFFERHEADERTYPE* buffer) {
  scoped_refptr<VideoFrame> frame;
  if (buffer->nFlags & OMX_BUFFERFLAG_EOS) {
    frame = VideoFrame::CreateEOSFrame();
  } else {
    OMX_VIDEO_PORTDEFINITIONTYPE* video_definition =
        reinterpret_cast<OMX_VIDEO_PORTDEFINITIONTYPE*>(buffer->pAppPrivate);
    DispmanxYUV420Resource* resource = resource_pool_->Alloc(
        video_definition->nStride, video_definition->nSliceHeight,
        video_definition->nFrameWidth, video_definition->nFrameHeight);
    if (!resource) {
      return NULL;
    }

    resource->WriteData(buffer->pBuffer);

    int64_t timestamp = ((buffer->nTimeStamp.nHighPart * 0x100000000ull) +
                         buffer->nTimeStamp.nLowPart);

    resource_pool_->AddRef();
    frame = new DispmanxVideoFrame(
        timestamp, resource,
        std::bind(&DispmanxResourcePool::DisposeDispmanxYUV420Resource,
                  resource_pool_, _1));
  }
  return frame;
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
