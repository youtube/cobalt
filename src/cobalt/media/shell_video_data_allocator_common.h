// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SHELL_VIDEO_DATA_ALLOCATOR_COMMON_H_
#define COBALT_MEDIA_SHELL_VIDEO_DATA_ALLOCATOR_COMMON_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "media/base/shell_video_data_allocator.h"

namespace media {

class ShellVideoDataAllocatorCommon : public ShellVideoDataAllocator {
 public:
  // When the requested frame size is less than |minimum_allocation_size|, the
  // class will still allocate |minimum_allocation_size|.  The same applies to
  // |minimum_alignment|.  When the requested size is larger than
  // |maximum_allocation_size|, the allocation will fail and return NULL.
  ShellVideoDataAllocatorCommon(
      cobalt::render_tree::ResourceProvider* resource_provider,
      size_t minimum_allocation_size, size_t maximum_allocation_size,
      size_t minimum_alignment);

  scoped_refptr<FrameBuffer> AllocateFrameBuffer(size_t size,
                                                 size_t alignment) override;
  scoped_refptr<VideoFrame> CreateYV12Frame(
      const scoped_refptr<FrameBuffer>& frame_buffer, const YV12Param& param,
      const base::TimeDelta& timestamp) override;

  scoped_refptr<VideoFrame> CreateNV12Frame(
      const scoped_refptr<FrameBuffer>& frame_buffer, const NV12Param& param,
      const base::TimeDelta& timestamp) override;

 private:
  typedef cobalt::render_tree::RawImageMemory RawImageMemory;

  class FrameBufferCommon : public FrameBuffer {
   public:
    explicit FrameBufferCommon(scoped_ptr<RawImageMemory> raw_image_memory);

    uint8* data() const override {
      return raw_image_memory_ ? raw_image_memory_->GetMemory() : NULL;
    }

    size_t size() const override {
      return raw_image_memory_ ? raw_image_memory_->GetSizeInBytes() : 0;
    }

    // Disown the image_data_.
    scoped_ptr<RawImageMemory> DetachRawImageMemory();

   private:
    scoped_ptr<RawImageMemory> raw_image_memory_;
  };

  cobalt::render_tree::ResourceProvider* resource_provider_;
  size_t minimum_allocation_size_;
  size_t maximum_allocation_size_;
  size_t minimum_alignment_;
};

}  // namespace media

#endif  // COBALT_MEDIA_SHELL_VIDEO_DATA_ALLOCATOR_COMMON_H_
