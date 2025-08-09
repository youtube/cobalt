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

#include "gpu/command_buffer/service/shared_image/starboard/starboard_video_image_backing.h"

#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

// Representation of StarboardVideoImageBacking as a GL Texture.
class StarboardVideoImageBacking::GLTexturePassthroughVideoImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughVideoImageRepresentation(
      SharedImageManager* manager,
      StarboardVideoImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<gpu::gles2::TexturePassthrough>> textures)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        passthrough_textures_(std::move(textures)) {
    for (auto& passthrough_texture : passthrough_textures_) {
      CHECK(passthrough_texture);
    }
  }

  ~GLTexturePassthroughVideoImageRepresentation() override {}

  // Disallow copy and assign.
  GLTexturePassthroughVideoImageRepresentation(
      const GLTexturePassthroughVideoImageRepresentation&) = delete;
  GLTexturePassthroughVideoImageRepresentation& operator=(
      const GLTexturePassthroughVideoImageRepresentation&) = delete;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override {
    DCHECK_LT(static_cast<size_t>(plane_index), passthrough_textures_.size());
    return passthrough_textures_[plane_index];
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    return true;
  }

  void EndAccess() override {}

 private:
  std::vector<scoped_refptr<gles2::TexturePassthrough>> passthrough_textures_;
};

StarboardVideoImageBacking::StarboardVideoImageBacking(
    const Mailbox& mailbox,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    std::vector<scoped_refptr<gpu::gles2::TexturePassthrough>> textures,
    scoped_refptr<SharedContextState> context_state)
    : ClearTrackingSharedImageBacking(
          mailbox,
          viz::MultiPlaneFormat::kYV12,
          size,
          color_space,
          surface_origin,
          alpha_type,
          (SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_GLES2),
          viz::MultiPlaneFormat::kYV12.EstimatedSizeInBytes(size),
          false),
      textures_(std::move(textures)),
      context_state_(std::move(context_state)),
      gpu_main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
}

StarboardVideoImageBacking::~StarboardVideoImageBacking() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  if (context_state_) {
    context_state_->RemoveContextLostObserver(this);
  }
  context_state_.reset();
}

void StarboardVideoImageBacking::OnContextLost() {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());

  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

SharedImageBackingType StarboardVideoImageBacking::GetType() const {
  return SharedImageBackingType::kVideo;
}

std::unique_ptr<SkiaGaneshImageRepresentation>
StarboardVideoImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(gpu_main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_state);

  if (!context_state->GrContextIsGL()) {
    DCHECK(false);
    return nullptr;
  }

  DCHECK(context_state->GrContextIsGL());
  DCHECK_EQ(textures_.size(), 3u);
  auto* texture_base = textures_[0].get();
  DCHECK(texture_base);
  const bool passthrough =
      (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough);
  DCHECK(passthrough);

  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures;
  for (auto& texture : textures_) {
    auto passthrough_texture = base::MakeRefCounted<gles2::TexturePassthrough>(
        texture->service_id(), GL_TEXTURE_EXTERNAL_OES);
    if (!passthrough_texture) {
      return nullptr;
    }
    textures.push_back(passthrough_texture);
  }

  std::unique_ptr<gpu::GLTextureImageRepresentationBase> gl_representation;
  gl_representation =
      std::make_unique<GLTexturePassthroughVideoImageRepresentation>(
          manager, this, tracker, std::move(textures));
  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

}  // namespace gpu
