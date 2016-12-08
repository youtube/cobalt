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

#include "cobalt/media/shell_media_platform_starboard.h"

#include <limits>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "media/audio/shell_audio_streamer.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_cached_decoder_buffer.h"
#include "starboard/configuration.h"
#include "starboard/media.h"

namespace media {

namespace {

const size_t kMediaBufferAlignment = SB_MEDIA_BUFFER_ALIGNMENT;
const size_t kVideoFrameAlignment = SB_MEDIA_VIDEO_FRAME_ALIGNMENT;

// This may be zero, which should disable GPU memory buffer allocations.
const size_t kGPUMemoryBufferBudget = SB_MEDIA_GPU_BUFFER_BUDGET;
const size_t kMainMemoryBufferBudget = SB_MEDIA_MAIN_BUFFER_BUDGET;

const size_t kSmallAllocationThreshold = 768U;

}  // namespace

ShellMediaPlatformStarboard::ShellMediaPlatformStarboard(
    cobalt::render_tree::ResourceProvider* resource_provider)
    : video_data_allocator_(resource_provider, 0,
                            std::numeric_limits<size_t>::max(),
                            kVideoFrameAlignment),
      video_frame_provider_(new ShellVideoFrameProvider) {
  DCHECK(!Instance());

  if (kGPUMemoryBufferBudget > 0) {
    gpu_memory_buffer_space_ = resource_provider->AllocateRawImageMemory(
        kGPUMemoryBufferBudget, kMediaBufferAlignment);
    DCHECK(gpu_memory_buffer_space_);
    DCHECK(gpu_memory_buffer_space_->GetMemory());
    DCHECK_GE(gpu_memory_buffer_space_->GetSizeInBytes(),
              kGPUMemoryBufferBudget);
    gpu_memory_pool_.reset(new nb::MemoryPool(
        gpu_memory_buffer_space_->GetMemory(),
        gpu_memory_buffer_space_->GetSizeInBytes(), true, /* thread_safe */
        true /* verify_full_capacity */, kSmallAllocationThreshold));
  }

  DCHECK_LE(0, kMainMemoryBufferBudget > 0);
  main_memory_buffer_space_.reset(static_cast<uint8*>(
      base::AlignedAlloc(kMainMemoryBufferBudget, kMediaBufferAlignment)));
  DCHECK(main_memory_buffer_space_);
  main_memory_pool_.reset(new nb::MemoryPool(main_memory_buffer_space_.get(),
                                             kMainMemoryBufferBudget,
                                             true, /* thread_safe */
                                             true, /* verify_full_capacity */
                                             kSmallAllocationThreshold));

  ShellBufferFactory::Initialize();
  ShellAudioStreamer::Initialize();

  SetInstance(this);
}

ShellMediaPlatformStarboard::~ShellMediaPlatformStarboard() {
  DCHECK_EQ(Instance(), this);
  SetInstance(NULL);

  ShellAudioStreamer::Terminate();
  ShellBufferFactory::Terminate();
}

void* ShellMediaPlatformStarboard::AllocateBuffer(size_t size) {
  if (kGPUMemoryBufferBudget) {
    return gpu_memory_pool_->Allocate(size, kMediaBufferAlignment);
  }

  return main_memory_pool_->Allocate(size, kMediaBufferAlignment);
}

void ShellMediaPlatformStarboard::FreeBuffer(void* ptr) {
  if (kGPUMemoryBufferBudget) {
    return gpu_memory_pool_->Free(ptr);
  }

  return main_memory_pool_->Free(ptr);
}

scoped_refptr<DecoderBuffer>
ShellMediaPlatformStarboard::ProcessBeforeLeavingDemuxer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!buffer || buffer->IsEndOfStream() || buffer->GetDataSize() == 0 ||
      !kGPUMemoryBufferBudget)
    return buffer;
  void* cached_buffer =
      main_memory_pool_->Allocate(buffer->GetDataSize(), kMediaBufferAlignment);
  if (!cached_buffer) return buffer;
  return new ShellCachedDecoderBuffer(
      buffer, cached_buffer,
      base::Bind(&nb::MemoryPool::Free,
                 base::Unretained(main_memory_pool_.get())));
}

bool ShellMediaPlatformStarboard::IsOutputProtected() {
  if (SbMediaIsOutputProtected()) {
    return true;
  }
  return SbMediaSetOutputProtection(true);
}

}  // namespace media
