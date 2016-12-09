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
const size_t kResourcePoolSize = 12;
const size_t kOMXOutputBufferCount = 4;
const int kMaxFrameWidth = 1920;
const int kMaxFrameHeight = 1088;
const size_t kMaxVideoFrameSize = kMaxFrameWidth * kMaxFrameHeight * 3 / 2;

}  // namespace

typedef OpenMaxVideoDecodeComponent::VideoFrame VideoFrame;

VideoFrameResourcePool::VideoFrameResourcePool(size_t max_number_of_resources)
    : max_number_of_resources_(max_number_of_resources),
      number_of_resources_(0),
      last_frame_height_(0) {}

VideoFrameResourcePool::~VideoFrameResourcePool() {
  for (ResourceMap::iterator iter = resource_map_.begin();
       iter != resource_map_.end(); ++iter) {
    while (!iter->second.empty()) {
      delete iter->second.front();
      iter->second.pop();
      --number_of_resources_;
    }
  }
  SB_DCHECK(number_of_resources_ == 0) << number_of_resources_;
}

DispmanxYUV420Resource* VideoFrameResourcePool::Alloc(int width,
                                                      int height,
                                                      int visible_width,
                                                      int visible_height) {
  ScopedLock scoped_lock(mutex_);

  last_frame_height_ = height;

  ResourceMap::iterator iter = resource_map_.find(height);
  if (iter != resource_map_.end() && !iter->second.empty()) {
    DispmanxYUV420Resource* resource = iter->second.front();
    iter->second.pop();
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
  if (resource->height() != last_frame_height_) {
    // The video has adapted, free the resource as it won't be reused any soon.
    delete resource;
    --number_of_resources_;
    return;
  }
  resource_map_[resource->height()].push(resource);
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
      resource_pool_(new VideoFrameResourcePool(kResourcePoolSize)) {
  OMXVideoParamPortFormat port_format;
  GetInputPortParam(&port_format);
  port_format.eCompressionFormat = OMX_VIDEO_CodingAVC;
  SetPortParam(port_format);
}

scoped_refptr<VideoFrame> OpenMaxVideoDecodeComponent::ReadVideoFrame() {
  if (OMX_BUFFERHEADERTYPE* buffer = PeekNextOutputBuffer()) {
    if (scoped_refptr<VideoFrame> frame = CreateVideoFrame(buffer)) {
      DropNextOutputBuffer();
      return frame;
    }
  }
  return NULL;
}

scoped_refptr<VideoFrame> OpenMaxVideoDecodeComponent::CreateVideoFrame(
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

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
