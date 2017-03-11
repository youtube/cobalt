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

#ifndef COBALT_RENDER_TREE_MESH_H_
#define COBALT_RENDER_TREE_MESH_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "starboard/types.h"
#include "third_party/glm/glm/vec2.hpp"
#include "third_party/glm/glm/vec3.hpp"

namespace cobalt {
namespace render_tree {

// Represents a mesh to which render_trees can be mapped for 3D projection
// by being applied a filter.
class Mesh : public base::RefCountedThreadSafe<Mesh> {
 public:
  // Vertices interleave position (x, y, z) and texture (u, v) coordinates.
  struct Vertex {
    glm::vec3 position_coord;
    glm::vec2 texture_coord;
  };
  COMPILE_ASSERT(sizeof(Vertex) == sizeof(float) * 5,
                 vertex_struct_size_exceeds_5_floats);

  // Defines a subset of the legal values for the draw mode parameter passed
  // into glDrawArrays() and glDrawElements(). These correspond to GL_TRIANGLES,
  // GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN. Users of this class can assert their
  // values correspond exactly to the GL constants in order to convert via
  // integer casting.
  enum DrawMode {
    kDrawModeTriangles = 4,
    kDrawModeTriangleStrip = 5,
    kDrawModeTriangleFan = 6
  };

  Mesh(const std::vector<Vertex>& vertices, const DrawMode mode,
       base::optional<uint32> crc = base::nullopt)
      : vertices_(vertices), draw_mode_(CheckDrawMode(mode)), crc_(crc) {}

  const std::vector<Vertex>& vertices() const { return vertices_; }

  DrawMode draw_mode() const { return draw_mode_; }

  const base::optional<uint32>& crc() const { return crc_; }

  uint32 GetEstimatedSizeInBytes() const {
    return static_cast<uint32>(vertices().size() * sizeof(Vertex) +
                               sizeof(DrawMode));
  }

  bool HasSameCrcAs(scoped_refptr<Mesh> other_mesh) const {
    return other_mesh && other_mesh->crc() == crc_;
  }

 protected:
  virtual ~Mesh() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<Mesh>;

 private:
  static DrawMode CheckDrawMode(DrawMode mode) {
    switch (mode) {
      case kDrawModeTriangles:
      case kDrawModeTriangleStrip:
      case kDrawModeTriangleFan:
        return mode;
      default:
        NOTREACHED() << "Unsupported render_tree::Mesh DrawMode detected, "
                        "defaulting to kDrawModeTriangleStrip";
        return kDrawModeTriangleStrip;
    }
  }

  const std::vector<Vertex> vertices_;
  const DrawMode draw_mode_;
  // Cyclic Redundancy Check code of the mesh projection box that contains this
  // mesh.
  const base::optional<uint32> crc_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MESH_H_
