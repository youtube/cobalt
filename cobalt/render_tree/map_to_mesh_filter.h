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

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/mesh.h"

namespace cobalt {
namespace render_tree {

enum StereoMode {
  kMono,
  // Stereoscopic modes where each half of the video represents the view of
  // one eye, and where the texture coordinates of the meshes need to be
  // scaled and offset to the appropriate half. The same mesh may be used for
  // both eyes, or there may be one for each eye.
  kLeftRight,
  kTopBottom,
  // Like kLeftRight, but where the texture coordinates already refer to the
  // corresponding half of the video and need no adjustment. This can only
  // happen when there are distinct meshes for each eye.
  kLeftRightUnadjustedTextureCoords
};

// A MapToMeshFilter can be used to map source content onto a 3D mesh, within a
// specified well-defined viewport. A null mesh indicates the equirectangular
// mesh should be used.
class MapToMeshFilter {
 public:
  MapToMeshFilter(StereoMode stereo_mode,
                  scoped_refptr<render_tree::Mesh> left_eye_mesh,
                  scoped_refptr<render_tree::Mesh> right_eye_mesh = NULL)
      : stereo_mode_(stereo_mode),
        left_eye_mesh_(left_eye_mesh),
        right_eye_mesh_(right_eye_mesh) {
    DCHECK(left_eye_mesh_);
    if (stereo_mode == kLeftRightUnadjustedTextureCoords) {
      // This stereo mode implies there are two meshes.
      DCHECK(left_eye_mesh_ && right_eye_mesh_);
    }
  }

  StereoMode stereo_mode() const { return stereo_mode_; }

  const scoped_refptr<render_tree::Mesh>& mono_mesh() const {
    return left_eye_mesh_;
  }

  const scoped_refptr<render_tree::Mesh>& left_eye_mesh() const {
    return left_eye_mesh_;
  }

  const scoped_refptr<render_tree::Mesh>& right_eye_mesh() const {
    return right_eye_mesh_;
  }

 private:
  StereoMode stereo_mode_;
  scoped_refptr<render_tree::Mesh> left_eye_mesh_;
  scoped_refptr<render_tree::Mesh> right_eye_mesh_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MAP_TO_MESH_FILTER_H_
