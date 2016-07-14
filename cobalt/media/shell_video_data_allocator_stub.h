/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SHELL_VIDEO_DATA_ALLOCATOR_STUB_H_
#define COBALT_MEDIA_SHELL_VIDEO_DATA_ALLOCATOR_STUB_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/shell_video_data_allocator.h"
#include "nb/memory_pool.h"

namespace media {

class ShellVideoDataAllocatorStub : public ShellVideoDataAllocator {
 public:
  explicit ShellVideoDataAllocatorStub(scoped_ptr<nb::MemoryPool> memory_pool);

  scoped_refptr<FrameBuffer> AllocateFrameBuffer(size_t size,
                                                 size_t alignment) OVERRIDE;
  scoped_refptr<VideoFrame> CreateYV12Frame(
      const scoped_refptr<FrameBuffer>& frame_buffer, const YV12Param& param,
      const base::TimeDelta& timestamp) OVERRIDE;

  scoped_refptr<VideoFrame> CreateNV12Frame(
      const scoped_refptr<FrameBuffer>& frame_buffer, const NV12Param& param,
      const base::TimeDelta& timestamp) OVERRIDE;

 private:
  class FrameBufferStub : public FrameBuffer {
   public:
    FrameBufferStub(nb::MemoryPool* memory_pool, size_t size, size_t alignment);
    ~FrameBufferStub();

    uint8* data() const OVERRIDE { return data_; }
    size_t size() const OVERRIDE { return size_; }

   private:
    nb::MemoryPool* memory_pool_;
    uint8* data_;
    size_t size_;
  };

  scoped_ptr<nb::MemoryPool> memory_pool_;
};

}  // namespace media

#endif  // COBALT_MEDIA_SHELL_VIDEO_DATA_ALLOCATOR_STUB_H_
