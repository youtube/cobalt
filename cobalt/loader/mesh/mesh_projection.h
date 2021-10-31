/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_MESH_MESH_PROJECTION_H_
#define COBALT_LOADER_MESH_MESH_PROJECTION_H_

#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "cobalt/render_tree/mesh.h"
#include "starboard/types.h"

namespace cobalt {
namespace loader {
namespace mesh {

// Represents a set of meshes to project a render subtree onto.
class MeshProjection : public base::RefCountedThreadSafe<MeshProjection> {
 public:
  // Represents a collection of meshes (vertex lists), each of which maps to a
  // texture ID. The specification currently only defines a valid ID of 0, with
  // all others reserved, so this vector should only have one mesh. In the
  // future more streams or static images could be represented by the texture
  // IDs.
  typedef std::vector<scoped_refptr<render_tree::Mesh>> MeshCollection;
  // Represents a list of mesh collections; in stereo mode, there is either
  // just one collection for both eyes (which undergoes an adjustment per eye)
  // or one for each eye. Mono video will only have one collection in the list.
  typedef std::vector<std::unique_ptr<MeshCollection>> MeshCollectionList;

  // Default mesh collection indices to GetMesh().
  enum CollectionIndex {
    kLeftEyeOrMonoCollection = 0,
    kRightEyeCollection = 1
  };

  MeshProjection(MeshCollectionList mesh_collections,
                 base::Optional<uint32> crc = base::nullopt)
      : mesh_collections_(std::move(mesh_collections)), crc_(crc) {
    DCHECK_GT(mesh_collections_.size(), 0UL);
    DCHECK_LE(mesh_collections_.size(), 2UL);
    // Check that the left-eye collection holds at least one mesh.
    DCHECK_GT(mesh_collections_[0]->size(), 0UL);
  }

  const base::Optional<uint32>& crc() const { return crc_; }

  // For stereo mode with distinct per-eye meshes, left eye is collection 0,
  // right is collection 1.
  scoped_refptr<render_tree::Mesh> GetMesh(size_t collection = 0) const {
    if (collection >= mesh_collections_.size()) {
      return NULL;
    }

    const MeshCollection& mesh_collection = *mesh_collections_[collection];

    CHECK(mesh_collection.size());

    return mesh_collection[0];
  }

  bool HasSeparateMeshPerEye() const { return mesh_collections_.size() == 2; }

  uint32 GetEstimatedSizeInBytes() const {
    uint32 estimate = sizeof(crc_);

    for (size_t i = 0; i < mesh_collections_.size(); i++) {
      for (size_t j = 0; j < mesh_collections_[i]->size(); j++) {
        estimate += mesh_collections_[i]->at(j)->GetEstimatedSizeInBytes();
      }
    }

    return estimate;
  }

  bool HasSameCrcAs(scoped_refptr<MeshProjection> other_meshproj) const {
    return other_meshproj && other_meshproj->crc() == crc_;
  }

 protected:
  virtual ~MeshProjection() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<MeshProjection>;

 private:
  const MeshCollectionList mesh_collections_;
  const base::Optional<uint32> crc_;
};

}  // namespace mesh
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MESH_MESH_PROJECTION_H_
