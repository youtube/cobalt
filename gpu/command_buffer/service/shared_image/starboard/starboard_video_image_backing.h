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

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_STARBOARD_STARBOARD_VIDEO_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_STARBOARD_STARBOARD_VIDEO_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
struct Mailbox;

// Implementation of ClearTrackingSharedImageBacking to allow
// StarboardRenderer to work in decode-to-texture mode for non-android
// platforms.
// TODO(b/427230150): Implement decode-to-texture mode on Linux.
class GPU_GLES2_EXPORT StarboardVideoImageBacking
    : public ClearTrackingSharedImageBacking,
      public SharedContextState::ContextLostObserver {
 public:
  StarboardVideoImageBacking(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      std::vector<scoped_refptr<gpu::gles2::TexturePassthrough>> textures,
      scoped_refptr<SharedContextState> context_state);

  ~StarboardVideoImageBacking() override;

  StarboardVideoImageBacking(const StarboardVideoImageBacking&) = delete;
  StarboardVideoImageBacking& operator=(const StarboardVideoImageBacking&) =
      delete;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 protected:
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class GLTexturePassthroughVideoImageRepresentation;

  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_main_task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_STARBOARD_STARBOARD_VIDEO_IMAGE_BACKING_H_
