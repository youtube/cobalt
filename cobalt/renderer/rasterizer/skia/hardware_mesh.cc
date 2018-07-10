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
  if (!vbo_) {
    rasterizer_message_loop_ = MessageLoop::current();
    vbo_.reset(new VertexBufferObject(vertices_.Pass(), draw_mode_));
  }

  return vbo_.get();
}

namespace {

void DestroyVBO(backend::GraphicsContextEGL* cobalt_context,
                scoped_ptr<VertexBufferObject> vbo) {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      cobalt_context);
  vbo.reset();
}

}  // namespace

HardwareMesh::~HardwareMesh() {
  if (rasterizer_message_loop_) {
    if (rasterizer_message_loop_ != MessageLoop::current()) {
      // Make sure that VBO cleanup always happens on the thread that created
      // the VBO in the first place.  We are passing cobalt_context_ by pointer
      // here, but we can assume it will still be alive when DestroyVBO is
      // executed because this Mesh object must  be destroyed before the
      // rasterizer, and the rasterizer must be destroyed before the GL
      // context.
      rasterizer_message_loop_->PostTask(
          FROM_HERE,
          base::Bind(&DestroyVBO, cobalt_context_, base::Passed(&vbo_)));
    } else {
      DestroyVBO(cobalt_context_, vbo_.Pass());
    }
  }
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
