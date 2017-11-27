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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_MESH_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_MESH_H_

#include <vector>

#include "cobalt/render_tree/mesh.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class SoftwareMesh : public render_tree::Mesh {
 public:
  SoftwareMesh(scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
               render_tree::Mesh::DrawMode draw_mode)
      : vertices_(vertices.Pass()), draw_mode_(draw_mode) {
    DCHECK(vertices_);
  }

  uint32 GetEstimatedSizeInBytes() const override {
    return static_cast<uint32>(vertices_->size() * 5 * sizeof(float) +
                               sizeof(DrawMode));
  }

  render_tree::Mesh::DrawMode GetDrawMode() const { return draw_mode_; }
  const std::vector<render_tree::Mesh::Vertex>& GetVertices() const {
    return *vertices_.get();
  }

 private:
  const scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices_;
  const render_tree::Mesh::DrawMode draw_mode_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_MESH_H_
