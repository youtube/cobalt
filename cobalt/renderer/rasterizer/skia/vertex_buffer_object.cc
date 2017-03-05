// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <vector>

#include "cobalt/renderer/rasterizer/skia/vertex_buffer_object.h"

#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

VertexBufferObject::VertexBufferObject(
    const scoped_refptr<render_tree::Mesh>& mesh)
    : mesh_(mesh) {
  DCHECK(mesh);
  const std::vector<render_tree::Mesh::Vertex>& vertices = mesh_->vertices();
  // Setup position and texture coordinate vertex buffer.
  GL_CALL(glGenBuffers(1, &mesh_vertex_buffer_));
  DLOG(INFO) << "Created VBO with buffer id " << mesh_vertex_buffer_;
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mesh_vertex_buffer_));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER,
                       vertices.size() * sizeof(vertices.front()),
                       &(vertices.front()), GL_STATIC_DRAW));
}

VertexBufferObject::~VertexBufferObject() {
  GL_CALL(glDeleteBuffers(1, &mesh_vertex_buffer_));
}

void VertexBufferObject::Bind() const {
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mesh_vertex_buffer_));
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
