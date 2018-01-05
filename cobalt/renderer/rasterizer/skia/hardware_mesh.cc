/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/hardware_mesh.h"

#include <vector>

#include "cobalt/renderer/backend/egl/graphics_context.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

uint32 HardwareMesh::GetEstimatedSizeInBytes() const {
  if (vertices_) {
    return static_cast<uint32>(vertices_->size() * sizeof(vertices_->front()) +
                               sizeof(draw_mode_));
  }
  return static_cast<uint32>(vbo_->GetVertexCount() * 5 * sizeof(float) +
                             sizeof(draw_mode_));
}

const VertexBufferObject* HardwareMesh::GetVBO() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!vbo_) {
    vbo_.reset(new VertexBufferObject(vertices_.Pass(), draw_mode_));
  }

  return vbo_.get();
}

HardwareMesh::~HardwareMesh() {
  // TODO: Support this function from being called from any thread.
  DCHECK(thread_checker_.CalledOnValidThread());
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      cobalt_context_);
  vbo_.reset();
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
