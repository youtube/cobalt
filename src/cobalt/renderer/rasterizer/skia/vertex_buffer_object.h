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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_VERTEX_BUFFER_OBJECT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_VERTEX_BUFFER_OBJECT_H_

#include <memory>
#include <vector>

#include "base/threading/thread_checker.h"
#include "cobalt/render_tree/mesh.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// A class for generating a Vertex Buffer Object stored on the GPU for an
// arbitrary vertex list.
class VertexBufferObject {
 public:
  // Creates a VertexBufferObject for the provided vertex list. The vertex list
  // object is destroyed from main memory, and a buffer will be allocated on
  // the GPU with its data.
  VertexBufferObject(
      std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
      GLenum draw_mode);
  // Cleans up the GPU memory that holds the vertices.
  virtual ~VertexBufferObject();

  void Bind() const;

  size_t GetVertexCount() const { return vertex_count_; }
  GLenum GetDrawMode() const { return draw_mode_; }
  GLuint GetHandle() const { return mesh_vertex_buffer_; }

 private:
  size_t vertex_count_;
  GLenum draw_mode_;
  GLuint mesh_vertex_buffer_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(VertexBufferObject);
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_VERTEX_BUFFER_OBJECT_H_
