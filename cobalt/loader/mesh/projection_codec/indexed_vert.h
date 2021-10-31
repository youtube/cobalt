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

#ifndef COBALT_LOADER_MESH_PROJECTION_CODEC_INDEXED_VERT_H_
#define COBALT_LOADER_MESH_PROJECTION_CODEC_INDEXED_VERT_H_

#include <stdint.h>

namespace cobalt {
namespace loader {
namespace mesh {
namespace projection_codec {

struct IndexedVert {
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t u;
  int32_t v;

  IndexedVert() : x(0), y(0), z(0), u(0), v(0) {}

  void operator+=(const IndexedVert& b);
  void operator-=(const IndexedVert& b);

  // Add a constant to all five indices
  void operator+=(int32_t b);

  // Subtract a constant from all five indices
  void operator-=(int32_t b);
};

}  // namespace projection_codec
}  // namespace mesh
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MESH_PROJECTION_CODEC_INDEXED_VERT_H_
