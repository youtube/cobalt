/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/media/shell_video_data_allocator_common.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"

namespace media {

namespace {

const int kMaxVideoWidth = 1920;
// Extra 8 pixels to align to 16.
const int kMaxVideoHeight = 1088;
const size_t kMaxYUVFrameSizeInBytes = kMaxVideoWidth * kMaxVideoHeight * 3 / 2;
const int kVideoFrameAlignment = 256;

void ReleaseImage(scoped_refptr<cobalt::render_tree::Image> image) {}
}

using cobalt::render_tree::ImageDataDescriptor;
using cobalt::render_tree::MultiPlaneImageDataDescriptor;
using cobalt::render_tree::kAlphaFormatUnpremultiplied;
using cobalt::render_tree::kMultiPlaneImageFormatYUV3PlaneBT709;
using cobalt::render_tree::kPixelFormatU8;
using cobalt::render_tree::kPixelFormatV8;
using cobalt::render_tree::kPixelFormatY8;

ShellVideoDataAllocatorCommon::ShellVideoDataAllocatorCommon(
    cobalt::render_tree::ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {}

scoped_refptr<ShellVideoDataAllocator::FrameBuffer>
ShellVideoDataAllocatorCommon::AllocateFrameBuffer(size_t size,
                                                   size_t alignment) {
  scoped_ptr<RawImageMemory> raw_image_memory =
      resource_provider_->AllocateRawImageMemory(kMaxYUVFrameSizeInBytes,
                                                 kVideoFrameAlignment);

  return raw_image_memory ? new FrameBufferCommon(raw_image_memory.Pass())
                          : NULL;
}

scoped_refptr<VideoFrame> ShellVideoDataAllocatorCommon::CreateYV12Frame(
    const scoped_refptr<FrameBuffer>& frame_buffer, const YV12Param& param,
    const base::TimeDelta& timestamp) {
  scoped_refptr<FrameBufferCommon> frame_buffer_common =
      base::polymorphic_downcast<FrameBufferCommon*>(frame_buffer.get());

  gfx::Size coded_size(param.decoded_width(), param.decoded_height());
  intptr_t offset = 0;
  gfx::Size plane_size(coded_size);
  int pitch_in_bytes = plane_size.width();

  DCHECK_EQ(pitch_in_bytes % 2, 0) << pitch_in_bytes << " has to be even.";

  // Create image data descriptor for the frame in I420.
  MultiPlaneImageDataDescriptor descriptor(
      kMultiPlaneImageFormatYUV3PlaneBT709);
  descriptor.AddPlane(
      offset, ImageDataDescriptor(plane_size, kPixelFormatY8,
                                  kAlphaFormatUnpremultiplied, pitch_in_bytes));
  offset += plane_size.width() * plane_size.height();
  plane_size.SetSize(plane_size.width() / 2, plane_size.height() / 2);
  pitch_in_bytes /= 2;
  descriptor.AddPlane(
      offset, ImageDataDescriptor(plane_size, kPixelFormatU8,
                                  kAlphaFormatUnpremultiplied, pitch_in_bytes));
  offset += plane_size.width() * plane_size.height();
  descriptor.AddPlane(
      offset, ImageDataDescriptor(plane_size, kPixelFormatV8,
                                  kAlphaFormatUnpremultiplied, pitch_in_bytes));
  offset += plane_size.width() * plane_size.height();
  CHECK_EQ(offset, coded_size.width() * coded_size.height() * 3 / 2);

  scoped_refptr<cobalt::render_tree::Image> image =
      resource_provider_->CreateMultiPlaneImageFromRawMemory(
          frame_buffer_common->DetachRawImageMemory(), descriptor);

  // The reference of the image is held by the closure that binds ReleaseImage,
  // so it won't be freed before the ReleaseImage is called.
  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapNativeTexture(
      reinterpret_cast<uintptr_t>(image.get()), 0, coded_size,
      param.visible_rect(), param.visible_rect().size(), timestamp,
      VideoFrame::ReadPixelsCB(), base::Bind(ReleaseImage, image));

  return video_frame;
}

ShellVideoDataAllocatorCommon::FrameBufferCommon::FrameBufferCommon(
    scoped_ptr<RawImageMemory> raw_image_memory)
    : raw_image_memory_(raw_image_memory.Pass()) {
  DCHECK(raw_image_memory_);
}

scoped_ptr<ShellVideoDataAllocatorCommon::RawImageMemory>
ShellVideoDataAllocatorCommon::FrameBufferCommon::DetachRawImageMemory() {
  DCHECK(raw_image_memory_);
  return raw_image_memory_.Pass();
}

}  // namespace media
