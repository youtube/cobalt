/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/skia/hardware_mesh.h"

#include <memory>
#include <vector>

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

uint32 HardwareMesh::GetEstimatedSizeInBytes() const {
  if (vertices_) {
    return static_cast<uint32>(vertices_->size() * sizeof(vertices_->front()) +
                               sizeof(draw_mode_));
  } else if (vbo_) {
    return static_cast<uint32>(vbo_->GetVertexCount() * 5 * sizeof(float) +
                               sizeof(draw_mode_));
  } else {
    return 0;
  }
}

const VertexBufferObject* HardwareMesh::GetVBO() const {
  if (!vbo_) {
    if (base::MessageLoop::current()) {
      rasterizer_task_runner_ = base::MessageLoop::current()->task_runner();
    }
    vbo_.reset(new VertexBufferObject(std::move(vertices_), draw_mode_));
  }

  return vbo_.get();
}

namespace {

void DestroyVBO(backend::GraphicsContextEGL* cobalt_context,
                std::unique_ptr<VertexBufferObject> vbo) {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      cobalt_context);
  vbo.reset();
}

}  // namespace

HardwareMesh::~HardwareMesh() {
  if (!vbo_) {
    return;
  }

  if (!rasterizer_task_runner_ ||
      rasterizer_task_runner_->BelongsToCurrentThread()) {
    DestroyVBO(cobalt_context_, std::move(vbo_));
    return;
  }

  // Make sure that VBO cleanup always happens on the thread that created
  // the VBO in the first place.  We are passing cobalt_context_ by pointer
  // here, but we can assume it will still be alive when DestroyVBO is
  // executed because this Mesh object must be destroyed before the
  // rasterizer, and the rasterizer must be destroyed before the GL
  // context.
  rasterizer_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DestroyVBO, cobalt_context_, base::Passed(&vbo_)));
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
