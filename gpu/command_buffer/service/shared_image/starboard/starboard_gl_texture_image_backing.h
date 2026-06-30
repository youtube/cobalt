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

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_STARBOARD_STARBOARD_GL_TEXTURE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_STARBOARD_STARBOARD_GL_TEXTURE_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "starboard/decode_target.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/ref_counted_lock.h"
#endif

namespace gpu {

namespace gles2 {
class Texture;
}

class GPU_GLES2_EXPORT StarboardGLTextureBacking
    : public ClearTrackingSharedImageBacking {
 public:
  StarboardGLTextureBacking(const Mailbox& mailbox,
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
  );

  ~StarboardGLTextureBacking() override;

  StarboardGLTextureBacking(const StarboardGLTextureBacking&) = delete;
  StarboardGLTextureBacking& operator=(const StarboardGLTextureBacking&) =
      delete;

  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

 protected:
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class GLTexturePassthroughStarboardImageRepresentation;

  std::vector<scoped_refptr<gpu::gles2::TexturePassthrough>>
      passthrough_textures_;

  SbDecodeTarget decode_target_ = kSbDecodeTargetInvalid;

#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<gpu::RefCountedLock> drdc_lock_;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_STARBOARD_STARBOARD_GL_TEXTURE_IMAGE_BACKING_H_
