// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_GPU_ANDROID_STARBOARD_STARBOARD_CODEC_IMAGE_H_
#define MEDIA_GPU_ANDROID_STARBOARD_STARBOARD_CODEC_IMAGE_H_

#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// Wrap TexturePassthrough to SharedImage, and allow
// AndroidVideoImageBacking to handle the texture.
class MEDIA_GPU_EXPORT StarboardCodecImage
    : public gpu::StreamTextureSharedImageInterface,
      gpu::RefCountedLockHelperDrDc {
 public:
  StarboardCodecImage(scoped_refptr<gpu::gles2::TexturePassthrough> texture,
                      scoped_refptr<gpu::RefCountedLock> drdc_lock);

  StarboardCodecImage(const StarboardCodecImage&) = delete;
  StarboardCodecImage& operator=(const StarboardCodecImage&) = delete;

  // gpu::StreamTextureSharedImageInterface implementation.
  void ReleaseResources() override;
  bool IsUsingGpuMemory() const override;
  void UpdateAndBindTexImage(GLuint service_id) override;
  bool HasTextureOwner() const override;
  gpu::TextureBase* GetTextureBase() const override;
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
  bool RenderToOverlay() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  bool TextureOwnerBindsTextureOnUpdate() override;

 protected:
  ~StarboardCodecImage() override;

 private:
  scoped_refptr<gpu::gles2::TexturePassthrough> texture_passthrough_;
  THREAD_CHECKER(gpu_main_thread_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_STARBOARD_STARBOARD_CODEC_IMAGE_H_
