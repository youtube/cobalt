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

// Represents a mesh (3D position + texture vertex list) to which render trees
// can be mapped for 3D projection by being applied a filter.
class Mesh : public base::RefCountedThreadSafe<Mesh> {
 public:
  // Vertices interleave position (x, y, z) and texture (u, v) coordinates.
  struct Vertex {
    float x;
    float y;
    float z;
    float u;
    float v;

    Vertex() : x(0.0f), y(0.0f), z(0.0f), u(0.0f), v(0.0f) {}
    Vertex(float x, float y, float z, float u, float v)
        : x(x), y(y), z(z), u(u), v(v) {}
  };
  COMPILE_ASSERT(sizeof(Vertex) == sizeof(float) * 5,
                 vertex_struct_size_exceeds_5_floats);

  // Defines how the mesh faces are constructed from the ordered list of
  // vertices.
  enum DrawMode {
    kDrawModeTriangles = 0,
    kDrawModeTriangleStrip = 1,
    kDrawModeTriangleFan = 2
  };

  virtual uint32 GetEstimatedSizeInBytes() const = 0;

 protected:
  virtual ~Mesh() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<Mesh>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MESH_H_
