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

#ifndef COBALT_MEDIA_SHELL_MEDIA_PLATFORM_STARBOARD_H_
#define COBALT_MEDIA_SHELL_MEDIA_PLATFORM_STARBOARD_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "cobalt/render_tree/resource_provider.h"

#if defined(COBALT_MEDIA_SOURCE_2016)

#include "cobalt/media/base/shell_media_platform.h"

namespace cobalt {
namespace media {

class ShellMediaPlatformStarboard : public ShellMediaPlatform {
 public:
  explicit ShellMediaPlatformStarboard(
      cobalt::render_tree::ResourceProvider* resource_provider)
      : resource_provider_(resource_provider) {
    SetInstance(this);
  }

  ~ShellMediaPlatformStarboard() {
    DCHECK_EQ(Instance(), this);
    SetInstance(NULL);
  }

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
#if SB_HAS(GRAPHICS)
    return resource_provider_->GetSbDecodeTargetGraphicsContextProvider();
#else   // SB_HAS(GRAPHICS)
    return NULL;
#endif  // SB_HAS(GRAPHICS)
  }

  void Suspend() override { resource_provider_ = NULL; }
  void Resume(render_tree::ResourceProvider* resource_provider) override {
    resource_provider_ = resource_provider;
  }

 private:
  void* AllocateBuffer(size_t size) override {
    UNREFERENCED_PARAMETER(size);
    NOTREACHED();
    return NULL;
  }
  void FreeBuffer(void* ptr) override {
    UNREFERENCED_PARAMETER(ptr);
    NOTREACHED();
  }
  size_t GetSourceBufferStreamAudioMemoryLimit() const override {
    NOTREACHED();
    return 0;
  }
  size_t GetSourceBufferStreamVideoMemoryLimit() const override {
    NOTREACHED();
    return 0;
  }

  cobalt::render_tree::ResourceProvider* resource_provider_;
};

}  // namespace media
}  // namespace cobalt

#else  // defined(COBALT_MEDIA_SOURCE_2016)

#include "media/base/shell_media_platform.h"

#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/media/shell_video_data_allocator_common.h"
#include "nb/memory_pool.h"
#include "starboard/common/locked_ptr.h"

namespace media {

class ShellMediaPlatformStarboard : public ShellMediaPlatform {
 public:
  ShellMediaPlatformStarboard(
      cobalt::render_tree::ResourceProvider* resource_provider);
  ~ShellMediaPlatformStarboard() override;

  void* AllocateBuffer(size_t size) override;
  void FreeBuffer(void* ptr) override;
  size_t GetSourceBufferStreamAudioMemoryLimit() const override {
    return SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT;
  }
  size_t GetSourceBufferStreamVideoMemoryLimit() const override {
    return SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT;
  }
  int GetMaxVideoPrerollFrames() const override {
    return SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES;
  }
  int GetMaxVideoFrames() const override {
    return SB_MEDIA_MAXIMUM_VIDEO_FRAMES;
  }
  scoped_refptr<DecoderBuffer> ProcessBeforeLeavingDemuxer(
      const scoped_refptr<DecoderBuffer>& buffer) override;
  bool IsOutputProtected() override;

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
#if SB_HAS(GRAPHICS)
    return resource_provider_->GetSbDecodeTargetGraphicsContextProvider();
#else   // SB_HAS(GRAPHICS)
    return NULL;
#endif  // SB_HAS(GRAPHICS)
  }

  void Suspend() override { resource_provider_ = NULL; }
  void Resume(
      cobalt::render_tree::ResourceProvider* resource_provider) override {
    resource_provider_ = resource_provider;
  }

 private:
  cobalt::render_tree::ResourceProvider* resource_provider_;

  // Optional GPU Memory buffer pool, for buffer offloading.
  scoped_ptr<cobalt::render_tree::RawImageMemory> gpu_memory_buffer_space_;
  starboard::LockedPtr<nb::MemoryPool> gpu_memory_pool_;

  // Main Memory buffer pool.
  scoped_ptr_malloc<uint8, base::ScopedPtrAlignedFree>
      main_memory_buffer_space_;
  starboard::LockedPtr<nb::MemoryPool> main_memory_pool_;

  ShellVideoDataAllocatorCommon video_data_allocator_;

  DISALLOW_COPY_AND_ASSIGN(ShellMediaPlatformStarboard);
};

}  // namespace media

#endif  // defined(COBALT_MEDIA_SOURCE_2016)

#endif  // COBALT_MEDIA_SHELL_MEDIA_PLATFORM_STARBOARD_H_
