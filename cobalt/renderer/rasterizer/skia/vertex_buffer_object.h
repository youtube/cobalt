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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_VERTEX_BUFFER_OBJECT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_VERTEX_BUFFER_OBJECT_H_

#include <GLES2/gl2.h>

#include "cobalt/render_tree/mesh.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// A class for generating a Vertex Buffer Object stored on the GPU for an
// arbitrary mesh. On creation, this will allocate a buffer on the GPU for the
// provided Mesh instance and on destruction it will cleanup that memory.
class VertexBufferObject {
 public:
  // Creates a VertexBufferObject for the provided mesh; this does not take
  // ownership over the mesh and should be destroyed when the mesh is destroyed.
  // Note this must be created after the GraphicsContext is set.
  explicit VertexBufferObject(const scoped_refptr<render_tree::Mesh>& mesh);
  virtual ~VertexBufferObject();

  void Bind() const;

  const scoped_refptr<render_tree::Mesh> GetMesh() const { return mesh_; }

  size_t GetNumVertices() const { return mesh_->vertices().size(); }

  GLuint GetHandle() const { return mesh_vertex_buffer_; }

 private:
  scoped_refptr<render_tree::Mesh> mesh_;
  GLuint mesh_vertex_buffer_;
  DISALLOW_COPY_AND_ASSIGN(VertexBufferObject);
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_VERTEX_BUFFER_OBJECT_H_
