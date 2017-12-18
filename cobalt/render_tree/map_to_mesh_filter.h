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

#ifndef COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_
#define COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/mesh.h"

namespace cobalt {
namespace render_tree {

enum StereoMode {
  kMono = 0,
  // Stereoscopic modes where each half of the content represents the view of
  // one eye, and where the texture coordinates of the meshes need to be
  // scaled and offset to the appropriate half. The same mesh may be used for
  // both eyes, or there may be one for each eye.
  kLeftRight = 1,
  kTopBottom = 2,
  // Like kLeftRight, but where the texture coordinates already refer to the
  // corresponding half of the content and need no adjustment. This can only
  // happen when there are distinct meshes for each eye.
  kLeftRightUnadjustedTextureCoords = 3
};

// The shape of the mesh that this filter applies.
enum MeshType {
  // Rectangular content: some rasterizers render rectangular video separately
  // from the rest of the document (for example, for stereo content). Does not
  // specify a custom mesh, since a rectangle with the content's aspect ratio
  // can be assumed (accounting for whether it needs to be halved in one
  // dimension for stereo). Not enabled on all rasterizers/platforms.
  kRectangular = 0,
  // Mesh that is centered around the camera; examples include 360 and 180
  // content. Needs to be specified, since the shape is arbitrary.
  kCustomMesh = 1
};

// A MapToMeshFilter can be used to map source content onto a 3D mesh, within a
// specified well-defined viewport.
class MapToMeshFilter {
 public:
  struct Builder {
    Builder() {}
    bool operator==(const Builder& other) const {
      return resolution_matched_meshes_ == other.resolution_matched_meshes_ &&
             left_eye_default_mesh_ == other.left_eye_default_mesh_ &&
             right_eye_default_mesh_ == other.right_eye_default_mesh_;
    }

    void SetDefaultMeshes(
        const scoped_refptr<render_tree::Mesh>& left_eye_mesh,
        const scoped_refptr<render_tree::Mesh>& right_eye_mesh) {
      left_eye_default_mesh_ = left_eye_mesh;
      right_eye_default_mesh_ = right_eye_mesh;
    }
    void AddResolutionMatchedMeshes(
        math::Size resolution,
        const scoped_refptr<render_tree::Mesh>& left_eye_mesh,
        const scoped_refptr<render_tree::Mesh>& right_eye_mesh) {
      DCHECK_GT(resolution.width(), 0);
      DCHECK_GT(resolution.height(), 0);
      DCHECK(left_eye_mesh);
      resolution_matched_meshes_[resolution] =
          std::make_pair(left_eye_mesh, right_eye_mesh);
    }
    const scoped_refptr<render_tree::Mesh>& left_eye_mesh(
        math::Size resolution) const {
      MeshMap::const_iterator match =
          resolution_matched_meshes_.find(resolution);
      if (match != resolution_matched_meshes_.end()) {
        return match->second.first;
      }
      return left_eye_default_mesh_;
    }
    const scoped_refptr<render_tree::Mesh>& right_eye_mesh(
        math::Size resolution) const {
      MeshMap::const_iterator match =
          resolution_matched_meshes_.find(resolution);
      if (match != resolution_matched_meshes_.end()) {
        return match->second.second;
      }
      return right_eye_default_mesh_;
    }

   private:
    struct SizeLessThan {
      bool operator()(const math::Size& a, const math::Size& b) const {
        return a.width() < b.width() ||
               (a.width() == b.width() && a.height() < b.height());
      }
    };
    typedef std::pair<scoped_refptr<render_tree::Mesh>,
                      scoped_refptr<render_tree::Mesh> > MeshPair;
    typedef std::map<math::Size, MeshPair, SizeLessThan> MeshMap;
    MeshMap resolution_matched_meshes_;
    scoped_refptr<render_tree::Mesh> left_eye_default_mesh_;
    scoped_refptr<render_tree::Mesh> right_eye_default_mesh_;
  };

  MapToMeshFilter(StereoMode stereo_mode, const Builder& builder)
      : stereo_mode_(stereo_mode), mesh_type_(kCustomMesh), data_(builder) {
    DCHECK(left_eye_mesh());
    if (stereo_mode == kLeftRightUnadjustedTextureCoords) {
      // This stereo mode implies there are two meshes.
      DCHECK(right_eye_mesh());
    }
  }

  // A filter without a an explicit mesh, to represent mesh-mapped content whose
  // mesh will be supplied externally.
  MapToMeshFilter(StereoMode stereo_mode, MeshType mesh_type)
      : stereo_mode_(stereo_mode), mesh_type_(mesh_type), data_() {
    DCHECK(mesh_type != kCustomMesh);
    DCHECK(stereo_mode != kLeftRightUnadjustedTextureCoords);
  }

  bool operator==(const MapToMeshFilter& other) const {
    return stereo_mode_ == other.stereo_mode_ && data_ == other.data_;
  }

  StereoMode stereo_mode() const { return stereo_mode_; }

  MeshType mesh_type() const { return mesh_type_; }

  // The omission of the |resolution| parameter will yield the default
  // meshes in each of the following functions (by failing to match the
  // invalid resolution).
  const scoped_refptr<render_tree::Mesh>& mono_mesh(
      math::Size resolution = InvalidSize()) const {
    return data_.left_eye_mesh(resolution);
  }

  const scoped_refptr<render_tree::Mesh>& left_eye_mesh(
      math::Size resolution = InvalidSize()) const {
    return data_.left_eye_mesh(resolution);
  }

  const scoped_refptr<render_tree::Mesh>& right_eye_mesh(
      math::Size resolution = InvalidSize()) const {
    return data_.right_eye_mesh(resolution);
  }

 private:
  static math::Size InvalidSize() { return math::Size(-1, -1); }
  StereoMode stereo_mode_;
  MeshType mesh_type_;
  Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_
