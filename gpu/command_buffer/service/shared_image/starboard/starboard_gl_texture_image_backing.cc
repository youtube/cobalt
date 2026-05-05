// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "gpu/command_buffer/service/shared_image/starboard/starboard_gl_texture_image_backing.h"

#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/ref_counted_lock.h"
#endif

namespace gpu {

class StarboardGLTextureBacking::
    GLTexturePassthroughStarboardImageRepresentation
    : public GLTexturePassthroughImageRepresentation
#if BUILDFLAG(IS_ANDROID)
    ,
      public RefCountedLockHelperDrDc
#endif
{
 public:
  GLTexturePassthroughStarboardImageRepresentation(
      SharedImageManager* manager,
      StarboardGLTextureBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<gpu::gles2::TexturePassthrough>> textures
#if BUILDFLAG(IS_ANDROID)
      ,
      scoped_refptr<RefCountedLock> drdc_lock
#endif
      )
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
#if BUILDFLAG(IS_ANDROID)
        RefCountedLockHelperDrDc(std::move(drdc_lock)),
#endif
        passthrough_textures_(std::move(textures)) {
    for (auto& passthrough_texture : passthrough_textures_) {
      CHECK(passthrough_texture);
    }
  }

  ~GLTexturePassthroughStarboardImageRepresentation() override = default;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    CHECK_LT(static_cast<size_t>(plane_index), passthrough_textures_.size());
    return passthrough_textures_[plane_index];
  }

  bool BeginAccess(GLenum mode) override NO_THREAD_SAFETY_ANALYSIS {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
#if BUILDFLAG(IS_ANDROID)
    if (GetDrDcLockPtr()) {
      GetDrDcLockPtr()->Acquire();
    }
#endif
    return true;
  }

  void EndAccess() override NO_THREAD_SAFETY_ANALYSIS {
#if BUILDFLAG(IS_ANDROID)
    if (GetDrDcLockPtr()) {
      GetDrDcLockPtr()->Release();
    }
#endif
  }

 private:
  std::vector<scoped_refptr<gles2::TexturePassthrough>> passthrough_textures_;
};

StarboardGLTextureBacking::StarboardGLTextureBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::vector<GLuint> texture_ids,
    std::vector<uint32_t> texture_targets,
    uint64_t decode_target
#if BUILDFLAG(IS_ANDROID)
    ,
    scoped_refptr<gpu::RefCountedLock> drdc_lock
#endif
    )
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      "StarboardGLTexture",
                                      format.EstimatedSizeInBytes(size),
                                      /*is_thread_safe=*/false),
      decode_target_(reinterpret_cast<SbDecodeTarget>(decode_target)) {
  for (size_t i = 0; i < texture_ids.size(); i++) {
    passthrough_textures_.push_back(
        base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
            texture_ids[i], texture_targets[i]));
  }
#if BUILDFLAG(IS_ANDROID)
  drdc_lock_ = std::move(drdc_lock);
#endif
  SetCleared();
}

StarboardGLTextureBacking::~StarboardGLTextureBacking() {
  // Mark context as lost to prevent glDeleteTextures calls.
  // The actual deletion of the underlying textures is handled by
  // SbDecodeTargetRelease below, since Starboard owns the texture resources.
  for (auto& passthrough_texture : passthrough_textures_) {
    if (passthrough_texture) {
      passthrough_texture->MarkContextLost();
    }
  }
  if (SbDecodeTargetIsValid(decode_target_)) {
    SbDecodeTargetRelease(decode_target_);
  }
}

SharedImageBackingType StarboardGLTextureBacking::GetType() const {
  return SharedImageBackingType::kGLTexture;
}

void StarboardGLTextureBacking::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
StarboardGLTextureBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<GLTexturePassthroughStarboardImageRepresentation>(
      manager, this, tracker, passthrough_textures_
#if BUILDFLAG(IS_ANDROID)
      ,
      drdc_lock_
#endif
  );
}

std::unique_ptr<SkiaGaneshImageRepresentation>
StarboardGLTextureBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  auto gl_representation = ProduceGLTexturePassthrough(manager, tracker);
  if (!gl_representation) {
    return nullptr;
  }
  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

}  // namespace gpu
