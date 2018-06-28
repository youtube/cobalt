// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MATH_MATRIX_INTERPOLATION_H_
#define COBALT_MATH_MATRIX_INTERPOLATION_H_

#include "base/memory/ref_counted.h"
#include "cobalt/math/matrix3_f.h"

namespace cobalt {
namespace math {

// This structure is based off of the specification provided by the W3C here:
//   https://www.w3.org/TR/css-transforms/#interpolation-of-2d-matrices
struct DecomposedMatrix {
  float translation[2];
  float scale[2];
  float angle;
  float m11;
  float m12;
  float m21;
  float m22;
};

// Decompose a matrix into basic transformations:
// (translation/scale/rotation/skew)
//   https://www.w3.org/TR/css-transforms/#decomposing-a-2d-matrix
DecomposedMatrix DecomposeMatrix(const math::Matrix3F& matrix);

// Interpolate between two decomposed matrices a and b by the specified
// progress.
//   https://www.w3.org/TR/css-transforms/#interpolation-of-decomposed-2d-matrix-values
DecomposedMatrix InterpolateMatrixDecompositions(const DecomposedMatrix& a,
                                                 const DecomposedMatrix& b,
                                                 float progress);

// Reconstruct a matrix from a given matrix decomposition.
//   https://www.w3.org/TR/css-transforms/#recomposing-to-a-2d-matrix
math::Matrix3F RecomposeMatrix(const DecomposedMatrix& decomposition);

// Interpolates from matrix a to matrix b according to the specified progress.
// The algorithm used to do this interpolation is described here:
//   https://www.w3.org/TR/css-transforms/#interpolation-of-2d-matrices
math::Matrix3F InterpolateMatrices(const math::Matrix3F& a,
                                   const math::Matrix3F& b, float progress);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_MATRIX_INTERPOLATION_H_
