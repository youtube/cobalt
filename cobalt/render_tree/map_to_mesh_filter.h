/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_
#define COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_

namespace cobalt {
namespace render_tree {

// A MapToMeshFilter can be used to map source content onto a 3D mesh, within a
// specified well-defined viewport.
class MapToMeshFilter {
 public:
  MapToMeshFilter() {}
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_
