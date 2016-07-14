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

#include "cobalt/media/shell_video_data_allocator_stub.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace media {

ShellVideoDataAllocatorStub::ShellVideoDataAllocatorStub(
    scoped_ptr<nb::MemoryPool> memory_pool)
    : memory_pool_(memory_pool.Pass()) {
  DCHECK(memory_pool_);
}

scoped_refptr<ShellVideoDataAllocator::FrameBuffer>
ShellVideoDataAllocatorStub::AllocateFrameBuffer(size_t size,
                                                 size_t alignment) {
  return new FrameBufferStub(memory_pool_.get(), size, alignment);
}

scoped_refptr<VideoFrame> ShellVideoDataAllocatorStub::CreateYV12Frame(
    const scoped_refptr<FrameBuffer>& frame_buffer, const YV12Param& param,
    const base::TimeDelta& timestamp) {
  UNREFERENCED_PARAMETER(frame_buffer);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(param.visible_rect().size());
  frame->SetTimestamp(timestamp);
  return frame;
}

scoped_refptr<VideoFrame> ShellVideoDataAllocatorStub::CreateNV12Frame(
    const scoped_refptr<FrameBuffer>& frame_buffer, const NV12Param& param,
    const base::TimeDelta& timestamp) {
  UNREFERENCED_PARAMETER(frame_buffer);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(param.visible_rect().size());
  frame->SetTimestamp(timestamp);
  return frame;
}

ShellVideoDataAllocatorStub::FrameBufferStub::FrameBufferStub(
    nb::MemoryPool* memory_pool, size_t size, size_t alignment)
    : memory_pool_(memory_pool), size_(size) {
  data_ = reinterpret_cast<uint8*>(memory_pool_->Allocate(size, alignment));
  DCHECK(data_);
}

ShellVideoDataAllocatorStub::FrameBufferStub::~FrameBufferStub() {
  memory_pool_->Free(data_);
}

}  // namespace media
