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

#include "media/gpu/android/starboard/starboard_codec_image.h"

#include "base/functional/callback.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"

namespace media {

StarboardCodecImage::StarboardCodecImage(
    scoped_refptr<gpu::gles2::TexturePassthrough> texture,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : RefCountedLockHelperDrDc(std::move(drdc_lock)),
      texture_passthrough_(std::move(texture)) {}

StarboardCodecImage::~StarboardCodecImage() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
}

void StarboardCodecImage::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
}

bool StarboardCodecImage::IsUsingGpuMemory() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_main_thread_checker_);
  AssertAcquiredDrDcLock();
  return true;
}

void StarboardCodecImage::UpdateAndBindTexImage(GLuint service_id) {
  AssertAcquiredDrDcLock();
  // TODO(borongchen): update decode-to-target
}

bool StarboardCodecImage::HasTextureOwner() const {
  return !!texture_passthrough_;
}

gpu::TextureBase* StarboardCodecImage::GetTextureBase() const {
  if (texture_passthrough_) {
    return texture_passthrough_.get();
  }
  return nullptr;
}

void StarboardCodecImage::NotifyOverlayPromotion(bool promotion,
                                                 const gfx::Rect& bounds) {
  AssertAcquiredDrDcLock();
}

bool StarboardCodecImage::RenderToOverlay() {
  return false;
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
StarboardCodecImage::GetAHardwareBuffer() {
  NOTREACHED() << "Don't use AHardwareBuffers with StarboardCodecImage";
}

bool StarboardCodecImage::TextureOwnerBindsTextureOnUpdate() {
  AssertAcquiredDrDcLock();
  return true;
}

}  // namespace media
