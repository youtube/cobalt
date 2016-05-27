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
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"

namespace media {

using cobalt::render_tree::Image;
using cobalt::render_tree::ImageData;
using cobalt::render_tree::ImageDataDescriptor;
using cobalt::render_tree::kAlphaFormatPremultiplied;
using cobalt::render_tree::kPixelFormatRGBA8;
using cobalt::render_tree::MultiPlaneImageDataDescriptor;
using cobalt::render_tree::ResourceProvider;
using cobalt::render_tree::kAlphaFormatUnpremultiplied;
using cobalt::render_tree::kMultiPlaneImageFormatYUV3PlaneBT709;
using cobalt::render_tree::kPixelFormatU8;
using cobalt::render_tree::kPixelFormatV8;
using cobalt::render_tree::kPixelFormatY8;

namespace {
void ReleaseImage(scoped_refptr<Image> /* image */) {}
}  // namespace

ShellVideoDataAllocatorCommon::ShellVideoDataAllocatorCommon(
    ResourceProvider* resource_provider, size_t minimum_allocation_size,
    size_t maximum_allocation_size, size_t minimum_alignment)
    : resource_provider_(resource_provider),
      minimum_allocation_size_(minimum_allocation_size),
      maximum_allocation_size_(maximum_allocation_size),
      minimum_alignment_(minimum_alignment) {}

scoped_refptr<ShellVideoDataAllocator::FrameBuffer>
ShellVideoDataAllocatorCommon::AllocateFrameBuffer(size_t size,
                                                   size_t alignment) {
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(alignment);
  DCHECK_LE(size, maximum_allocation_size_);
  if (size > maximum_allocation_size_) {
    NOTREACHED();
    return NULL;
  }
  size = std::max(size, minimum_allocation_size_);
  alignment = std::max(alignment, minimum_alignment_);
  scoped_ptr<RawImageMemory> raw_image_memory =
      resource_provider_->AllocateRawImageMemory(size, alignment);

  return raw_image_memory ? new FrameBufferCommon(raw_image_memory.Pass())
                          : NULL;
}

scoped_refptr<VideoFrame> ShellVideoDataAllocatorCommon::CreateYV12Frame(
    const scoped_refptr<FrameBuffer>& frame_buffer, const YV12Param& param,
    const base::TimeDelta& timestamp) {
  scoped_refptr<FrameBufferCommon> frame_buffer_common =
      base::polymorphic_downcast<FrameBufferCommon*>(frame_buffer.get());

  // TODO(***REMOVED***): Ensure it work with visible_rect with non-zero left and
  // top.  Note that simply add offset to the image buffer may cause alignment
  // issues.
  gfx::Size decoded_size(param.decoded_width(), param.decoded_height());
  gfx::Size visible_size(param.visible_rect().size());

  intptr_t offset = 0;
  int pitch_in_bytes = decoded_size.width();

  DCHECK_EQ(pitch_in_bytes % 2, 0) << pitch_in_bytes << " has to be even.";

  // Create image data descriptor for the frame in I420.
  MultiPlaneImageDataDescriptor descriptor(
      kMultiPlaneImageFormatYUV3PlaneBT709);
  descriptor.AddPlane(
      offset, ImageDataDescriptor(visible_size, kPixelFormatY8,
                                  kAlphaFormatUnpremultiplied, pitch_in_bytes));
  offset += decoded_size.width() * decoded_size.height();
  visible_size.SetSize(visible_size.width() / 2, visible_size.height() / 2);
  pitch_in_bytes /= 2;
  descriptor.AddPlane(
      offset, ImageDataDescriptor(visible_size, kPixelFormatU8,
                                  kAlphaFormatUnpremultiplied, pitch_in_bytes));
  offset += decoded_size.width() / 2 * decoded_size.height() / 2;
  descriptor.AddPlane(
      offset, ImageDataDescriptor(visible_size, kPixelFormatV8,
                                  kAlphaFormatUnpremultiplied, pitch_in_bytes));
  offset += decoded_size.width() / 2 * decoded_size.height() / 2;
  CHECK_EQ(offset, param.decoded_width() * param.decoded_height() * 3 / 2);

  scoped_refptr<Image> image =
      resource_provider_->CreateMultiPlaneImageFromRawMemory(
          frame_buffer_common->DetachRawImageMemory(), descriptor);

  // The reference of the image is held by the closure that binds ReleaseImage,
  // so it won't be freed before the ReleaseImage is called.
  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapNativeTexture(
      reinterpret_cast<uintptr_t>(image.get()), 0, visible_size,
      param.visible_rect(), visible_size, timestamp, VideoFrame::ReadPixelsCB(),
      base::Bind(ReleaseImage, image));
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
