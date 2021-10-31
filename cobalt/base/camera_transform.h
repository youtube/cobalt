// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_CAMERA_TRANSFORM_H_
#define COBALT_BASE_CAMERA_TRANSFORM_H_

#include "base/optional.h"
#include "third_party/glm/glm/mat4x4.hpp"

namespace base {

// CameraTransform holds the affine transformations needed to apply a
// camera viewing a 3D scene.
class CameraTransform {
 public:
  struct ProjectionAndView {
    glm::mat4 projection_matrix;
    glm::mat4 view_matrix;
  };

  ProjectionAndView left_eye_or_mono;
  base::Optional<ProjectionAndView> right_eye;  // For stereoscopic cameras.

  CameraTransform(glm::mat4 left_eye_or_mono_projection_matrix,
                  glm::mat4 left_eye_or_mono_view_matrix,
                  glm::mat4 right_eye_projection_matrix,
                  glm::mat4 right_eye_view_matrix) {
    left_eye_or_mono.projection_matrix = left_eye_or_mono_projection_matrix;
    left_eye_or_mono.view_matrix = left_eye_or_mono_view_matrix;
    right_eye = ProjectionAndView();
    right_eye->projection_matrix = right_eye_projection_matrix;
    right_eye->view_matrix = right_eye_view_matrix;
  }

  CameraTransform(glm::mat4 left_eye_or_mono_projection_matrix,
                  glm::mat4 left_eye_or_mono_view_matrix) {
    left_eye_or_mono.projection_matrix = left_eye_or_mono_projection_matrix;
    left_eye_or_mono.view_matrix = left_eye_or_mono_view_matrix;
  }
};

}  // namespace base

#endif  // COBALT_BASE_CAMERA_TRANSFORM_H_
