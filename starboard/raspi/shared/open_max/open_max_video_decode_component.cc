// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/raspi/shared/open_max/open_max_video_decode_component.h"

#include <algorithm>

#include "starboard/configuration.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

const char kVideoDecodeComponentName[] = "OMX.broadcom.video_decode";
const size_t kResourcePoolSize = 26;
const size_t kOMXOutputBufferCount = 12;
const int kMaxFrameWidth = 1920;
const int kMaxFrameHeight = 1088;
const size_t kMaxVideoFrameSize = kMaxFrameWidth * kMaxFrameHeight * 3 / 2;

typedef OMXParam<OMX_VIDEO_PARAM_PORTFORMATTYPE, OMX_IndexParamVideoPortFormat>
    OMXVideoParamPortFormat;

}  // namespace

typedef OpenMaxVideoDecodeComponent::VideoFrame VideoFrame;

VideoFrameResourcePool::VideoFrameResourcePool(size_t max_number_of_resources)
    : max_number_of_resources_(max_number_of_resources),
      number_of_resources_(0),
      last_frame_width_(0),
      last_frame_height_(0) {}

VideoFrameResourcePool::~VideoFrameResourcePool() {
  while (!free_resources_.empty()) {
    delete free_resources_.front();
    free_resources_.pop();
    --number_of_resources_;
  }
  SB_DCHECK(number_of_resources_ == 0) << number_of_resources_;
}

DispmanxYUV420Resource* VideoFrameResourcePool::Alloc(int width,
                                                      int height,
                                                      int visible_width,
                                                      int visible_height) {
  ScopedLock scoped_lock(mutex_);

  if (last_frame_width_ != width || last_frame_height_ != height) {
    while (!free_resources_.empty()) {
      delete free_resources_.front();
      free_resources_.pop();
      --number_of_resources_;
    }
  }

  last_frame_width_ = width;
  last_frame_height_ = height;

  if (!free_resources_.empty()) {
    DispmanxYUV420Resource* resource = free_resources_.front();
    free_resources_.pop();
    return resource;
  }

  if (number_of_resources_ >= max_number_of_resources_) {
    return NULL;
  }

  ++number_of_resources_;
  return new DispmanxYUV420Resource(width, height, visible_width,
                                    visible_height);
}

void VideoFrameResourcePool::Free(DispmanxYUV420Resource* resource) {
  ScopedLock scoped_lock(mutex_);
  if (resource->width() != last_frame_width_ ||
      resource->height() != last_frame_height_) {
    // The video has adapted, free the resource as it won't be reused any soon.
    delete resource;
    --number_of_resources_;
    return;
  }
  free_resources_.push(resource);
}

// static
void VideoFrameResourcePool::DisposeDispmanxYUV420Resource(
    void* context,
    void* dispmanx_resource) {
  SB_DCHECK(context != NULL);
  SB_DCHECK(dispmanx_resource != NULL);
  VideoFrameResourcePool* pool =
      reinterpret_cast<VideoFrameResourcePool*>(context);
  pool->Free(reinterpret_cast<DispmanxYUV420Resource*>(dispmanx_resource));
  pool->Release();
}

OpenMaxVideoDecodeComponent::OpenMaxVideoDecodeComponent()
    : OpenMaxComponent(kVideoDecodeComponentName),
      resource_pool_(new VideoFrameResourcePool(kResourcePoolSize)),
      frame_creator_thread_(kSbThreadInvalid),
      buffer_filled_condition_variable_(mutex_),
      kill_frame_creator_thread_(false) {
  OMXVideoParamPortFormat port_format;
  GetInputPortParam(&port_format);
  port_format.eCompressionFormat = OMX_VIDEO_CodingAVC;
  SetPortParam(port_format);
}

OpenMaxVideoDecodeComponent::~OpenMaxVideoDecodeComponent() {
  Flush();
}

scoped_refptr<VideoFrame> OpenMaxVideoDecodeComponent::ReadFrame() {
  if (!SbThreadIsValid(frame_creator_thread_)) {
    SB_DCHECK(!kill_frame_creator_thread_);
    frame_creator_thread_ = SbThreadCreate(
        0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
        "omx_video_decoder",
        &OpenMaxVideoDecodeComponent::FrameCreatorThreadEntryPoint, this);
  }

  ScopedLock scoped_lock(mutex_);
  if (decoded_frames_.empty()) {
    return NULL;
  }
  scoped_refptr<VideoFrame> frame = decoded_frames_.front();
  decoded_frames_.pop();
  return frame;
}

void OpenMaxVideoDecodeComponent::Flush() {
  if (SbThreadIsValid(frame_creator_thread_)) {
    {
      ScopedLock scoped_lock(mutex_);
      kill_frame_creator_thread_ = true;
      buffer_filled_condition_variable_.Signal();
    }
    SbThreadJoin(frame_creator_thread_, NULL);
    frame_creator_thread_ = kSbThreadInvalid;
    kill_frame_creator_thread_ = false;
  }

  while (!decoded_frames_.empty()) {
    decoded_frames_.pop();
  }

  OpenMaxComponent::Flush();
}

scoped_refptr<VideoFrame> OpenMaxVideoDecodeComponent::CreateFrame(
    OMX_BUFFERHEADERTYPE* buffer) {
  scoped_refptr<VideoFrame> frame;
  if (buffer->nFlags & OMX_BUFFERFLAG_EOS) {
    frame = VideoFrame::CreateEOSFrame();
  } else {
    OMX_VIDEO_PORTDEFINITIONTYPE& video_definition =
        output_port_definition_.format.video;
    DispmanxYUV420Resource* resource = resource_pool_->Alloc(
        video_definition.nStride, video_definition.nSliceHeight,
        video_definition.nFrameWidth, video_definition.nFrameHeight);
    if (!resource) {
      return NULL;
    }

    resource->WriteData(buffer->pBuffer);

    SbMediaTime timestamp = ((buffer->nTimeStamp.nHighPart * 0x100000000ull) +
                             buffer->nTimeStamp.nLowPart) *
                            kSbMediaTimeSecond / kSbTimeSecond;

    resource_pool_->AddRef();
    frame = new VideoFrame(
        video_definition.nFrameWidth, video_definition.nFrameHeight, timestamp,
        resource, resource_pool_,
        &VideoFrameResourcePool::DisposeDispmanxYUV420Resource);
  }
  return frame;
}

bool OpenMaxVideoDecodeComponent::OnEnableOutputPort(
    OMXParamPortDefinition* port_definition) {
  SB_DCHECK(port_definition);

  output_port_definition_ = *port_definition;
  SB_DCHECK(port_definition->format.video.eColorFormat ==
            OMX_COLOR_FormatYUV420PackedPlanar);
  port_definition->format.video.eColorFormat =
      OMX_COLOR_FormatYUV420PackedPlanar;
  port_definition->nBufferCountActual = kOMXOutputBufferCount;
  port_definition->nBufferSize =
      std::max(port_definition->nBufferSize, kMaxVideoFrameSize);
  return true;
}

void OpenMaxVideoDecodeComponent::OnReadyToPeekOutputBuffer() {
  ScopedLock scoped_lock(mutex_);
  buffer_filled_condition_variable_.Signal();
}

void OpenMaxVideoDecodeComponent::CreateFrames() {
  for (;;) {
    OMX_BUFFERHEADERTYPE* buffer = PeekNextOutputBuffer();
    {
      ScopedLock scoped_lock(mutex_);
      if (kill_frame_creator_thread_) {
        return;
      }
      if (!buffer) {
        buffer_filled_condition_variable_.Wait();
        continue;
      }
    }
    if (scoped_refptr<VideoFrame> frame = CreateFrame(buffer)) {
      DropNextOutputBuffer();
      ScopedLock scoped_lock(mutex_);
      decoded_frames_.push(frame);
    } else {
      // This indicates that there is a very large frame backlog and it is safe
      // to sleep for a while in this case.
      SbThreadSleep(kSbTimeMillisecond * 10);
    }
  }
}

// static
void* OpenMaxVideoDecodeComponent::FrameCreatorThreadEntryPoint(void* context) {
  OpenMaxVideoDecodeComponent* component =
      reinterpret_cast<OpenMaxVideoDecodeComponent*>(context);
  component->CreateFrames();
  return NULL;
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
