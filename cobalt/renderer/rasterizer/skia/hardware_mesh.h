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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_MESH_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_MESH_H_

#include <vector>

#include "base/threading/thread_checker.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/rasterizer/skia/vertex_buffer_object.h"
#include "third_party/glm/glm/vec2.hpp"
#include "third_party/glm/glm/vec3.hpp"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class HardwareMesh : public render_tree::Mesh {
 public:
  HardwareMesh(scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
               DrawMode draw_mode, backend::GraphicsContextEGL* cobalt_context)
      : vertices_(vertices.Pass()),
        draw_mode_(CheckDrawMode(draw_mode)),
        cobalt_context_(cobalt_context) {
    DCHECK(vertices_);
    thread_checker_.DetachFromThread();
  }

  uint32 GetEstimatedSizeInBytes() const override;

  // Float array of vertices, contiguously interleaved X, Y, Z, U, V coords.
  const float* GetVertices() const {
    DCHECK(vertices_ && vertices_->size());
    return reinterpret_cast<const float*>(vertices_->data());
  }

  const size_t GetVertexCount() const {
    DCHECK(vertices_);
    return vertices_->size();
  }

  const render_tree::Mesh::DrawMode GetDrawMode() const {
    return draw_mode_ == GL_TRIANGLE_FAN
               ? render_tree::Mesh::DrawMode::kDrawModeTriangleFan
               : draw_mode_ == GL_TRIANGLE_STRIP
                     ? render_tree::Mesh::DrawMode::kDrawModeTriangleStrip
                     : render_tree::Mesh::DrawMode::kDrawModeTriangles;
  }

  // Obtains a vertex buffer object from this mesh. Called right before first
  // rendering it so that the graphics context has already been made current.
  const VertexBufferObject* GetVBO() const;

  ~HardwareMesh() override;

 private:
  static GLenum CheckDrawMode(DrawMode mode) {
    switch (mode) {
      case kDrawModeTriangles:
        return GL_TRIANGLES;
      case kDrawModeTriangleStrip:
        return GL_TRIANGLE_STRIP;
      case kDrawModeTriangleFan:
        return GL_TRIANGLE_FAN;
      default:
        NOTREACHED() << "Unsupported Mesh DrawMode detected, "
                        "defaulting to GL_TRIANGLE_STRIP";
        return GL_TRIANGLE_STRIP;
    }
  }

  // Logically the mesh is the same, but internally upon fetching the VBO,
  // the vertex list has been copied from CPU to GPU memory.
  mutable scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices_;
  mutable scoped_ptr<VertexBufferObject> vbo_;
  const GLenum draw_mode_;
  backend::GraphicsContextEGL* cobalt_context_;

  base::ThreadChecker thread_checker_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_MESH_H_
