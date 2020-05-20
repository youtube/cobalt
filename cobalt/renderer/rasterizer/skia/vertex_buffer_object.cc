// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include <memory>
#include <vector>

#include "cobalt/renderer/rasterizer/skia/vertex_buffer_object.h"

#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

VertexBufferObject::VertexBufferObject(
    std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
    GLenum draw_mode)
    : vertex_count_(vertices->size()), draw_mode_(draw_mode) {
  std::unique_ptr<float[]> buffer(new float[5 * vertex_count_]);

  for (size_t i = 0; i < vertices->size(); i++) {
    buffer[5 * i] = vertices->at(i).x;
    buffer[5 * i + 1] = vertices->at(i).y;
    buffer[5 * i + 2] = vertices->at(i).z;
    buffer[5 * i + 3] = vertices->at(i).u;
    buffer[5 * i + 4] = vertices->at(i).v;
  }

  // Setup position and texture coordinate vertex buffer.
  GL_CALL(glGenBuffers(1, &mesh_vertex_buffer_));
  DLOG(INFO) << "Created VBO with buffer id " << mesh_vertex_buffer_;
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mesh_vertex_buffer_));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, vertex_count_ * 5 * sizeof(float),
                       buffer.get(), GL_STATIC_DRAW));

  // Vertex list data object is deleted at this point.
}

VertexBufferObject::~VertexBufferObject() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DLOG(INFO) << "Deleted VBO with buffer id " << mesh_vertex_buffer_;
  GL_CALL(glDeleteBuffers(1, &mesh_vertex_buffer_));
}

void VertexBufferObject::Bind() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, mesh_vertex_buffer_));
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
