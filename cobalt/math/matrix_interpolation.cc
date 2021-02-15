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

#include "cobalt/math/matrix_interpolation.h"

#include <cmath>

#include "cobalt/math/transform_2d.h"

namespace cobalt {
namespace math {

// All algorithms here are based on the specifications provided by the W3C here:
//   https://www.w3.org/TR/css-transforms/#interpolation-of-2d-matrices
DecomposedMatrix DecomposeMatrix(const math::Matrix3F& matrix) {
  DecomposedMatrix decomposition;

  float col0x = matrix.Get(0, 0);
  float col0y = matrix.Get(1, 0);
  float col1x = matrix.Get(0, 1);
  float col1y = matrix.Get(1, 1);

  decomposition.translation[0] = matrix.Get(0, 2);
  decomposition.translation[1] = matrix.Get(1, 2);

  decomposition.scale[0] = sqrt(col0x * col0x + col0y * col0y);
  decomposition.scale[1] = sqrt(col1x * col1x + col1y * col1y);

  // If determinant is negative, one axis was flipped.
  float determinant = col0x * col1y - col0y * col1x;
  if (determinant < 0) {
    // Flip axis with minimum unit vector dot product.
    if (col0x < col1y) {
      decomposition.scale[0] = -decomposition.scale[0];
    } else {
      decomposition.scale[1] = -decomposition.scale[1];
    }
  }

  // Renormalize matrix to remove scale.
  if ((0.0f != decomposition.scale[0])) {
    col0x *= 1 / decomposition.scale[0];
    col0y *= 1 / decomposition.scale[0];
  }
  if ((0.0f != decomposition.scale[1])) {
    col1x *= 1 / decomposition.scale[1];
    col1y *= 1 / decomposition.scale[1];
  }

  // Compute rotation and renormalize matrix.
  decomposition.angle = atan2(col0y, col0x);

  if ((0.0f != decomposition.angle)) {
    // Rotate(-angle) = [ cos(angle),  sin(angle) ]
    //                  [ -sin(angle), cos(angle) ]
    //                = [ col0x,  col0y ]
    //                  [ -col0y, col0x ]
    // Thanks to the normalization above.
    float sin_angle = col0y;
    float cos_angle = col0x;
    float m11 = col0x;
    float m12 = col1x;
    float m21 = col0y;
    float m22 = col1y;
    col0x = cos_angle * m11 + sin_angle * m21;
    col0y = -sin_angle * m11 + cos_angle * m21;
    col1x = cos_angle * m12 + sin_angle * m22;
    col1y = -sin_angle * m12 + cos_angle * m22;
  }

  decomposition.m11 = col0x;
  decomposition.m12 = col1x;
  decomposition.m21 = col0y;
  decomposition.m22 = col1y;

  return decomposition;
}

float Lerp(float a, float b, float progress) { return a + (b - a) * progress; }

namespace {

// Prepare certain attributes of the decomposed matrices for interpolation.
void SanitizeInputsForInterpolation(const DecomposedMatrix& a,
                                    const DecomposedMatrix& b,
                                    float* a_scale_sanitized,
                                    float* a_angle_sanitized,
                                    float* b_angle_sanitized) {
  static const float kPi = static_cast<float>(M_PI);

  // If x-axis of one is flipped, and y-axis of the other,
  // convert to an unflipped rotation.
  if ((a.scale[0] < 0 && b.scale[1] < 0) ||
      (a.scale[1] < 0 && b.scale[0] < 0)) {
    a_scale_sanitized[0] = -a.scale[0];
    a_scale_sanitized[1] = -a.scale[1];
    *a_angle_sanitized = a.angle + a.angle < 0 ? kPi : -kPi;
  } else {
    a_scale_sanitized[0] = a.scale[0];
    a_scale_sanitized[1] = a.scale[1];
    *a_angle_sanitized = a.angle;
  }

  // Don’t rotate the long way around.
  *b_angle_sanitized = b.angle;
  float angle_difference = std::abs(*a_angle_sanitized - *b_angle_sanitized);
  if (angle_difference > kPi) {
    float rounded_up_revolutions_difference =
        std::ceil(angle_difference / (2 * kPi));
    if (*a_angle_sanitized > *b_angle_sanitized) {
      *a_angle_sanitized -= 2 * kPi * rounded_up_revolutions_difference;
    } else {
      *b_angle_sanitized -= 2 * kPi * rounded_up_revolutions_difference;
    }
  }
}

}  // namespace

DecomposedMatrix InterpolateMatrixDecompositions(const DecomposedMatrix& a,
                                                 const DecomposedMatrix& b,
                                                 float progress) {
  float a_scale[2];
  float a_angle;
  float b_angle;
  SanitizeInputsForInterpolation(a, b, a_scale, &a_angle, &b_angle);

  // And now we lerp each component individually.
  DecomposedMatrix result;
  result.translation[0] = Lerp(a.translation[0], b.translation[0], progress);
  result.translation[1] = Lerp(a.translation[1], b.translation[1], progress);
  result.scale[0] = Lerp(a_scale[0], b.scale[0], progress);
  result.scale[1] = Lerp(a_scale[1], b.scale[1], progress);
  result.angle = Lerp(a_angle, b_angle, progress);
  result.m11 = Lerp(a.m11, b.m11, progress);
  result.m12 = Lerp(a.m12, b.m12, progress);
  result.m21 = Lerp(a.m21, b.m21, progress);
  result.m22 = Lerp(a.m22, b.m22, progress);
  return result;
}

// Reconstruct a matrix from a given matrix decomposition.
//   https://www.w3.org/TR/css-transforms/#recomposing-to-a-2d-matrix
math::Matrix3F RecomposeMatrix(const DecomposedMatrix& decomposition) {
  math::Matrix3F matrix(math::Matrix3F::Identity());

  matrix(0, 0) = decomposition.m11;
  matrix(0, 1) = decomposition.m12;
  matrix(1, 0) = decomposition.m21;
  matrix(1, 1) = decomposition.m22;

  // Rotate matrix.
  matrix = math::RotateMatrix(-decomposition.angle) * matrix;

  // Scale matrix.
  matrix(0, 0) *= decomposition.scale[0];
  matrix(1, 0) *= decomposition.scale[0];
  matrix(0, 1) *= decomposition.scale[1];
  matrix(1, 1) *= decomposition.scale[1];

  // Translate matrix.  Note that we deviate from the specification here
  // because the specification's "recompose" algorithm is inconsistent with
  // its "decompose" algorithm.  The modification achieves this consistency.
  matrix(0, 2) = decomposition.translation[0];
  matrix(1, 2) = decomposition.translation[1];

  return matrix;
}

math::Matrix3F InterpolateMatrices(const math::Matrix3F& a,
                                   const math::Matrix3F& b, float progress) {
  // The main algorithm here is to decompose each matrix, interpolate the
  // decompositions, and then reconstruct a matrix from the interpolated
  // compositions.
  DecomposedMatrix decomposed_a = DecomposeMatrix(a);
  DecomposedMatrix decomposed_b = DecomposeMatrix(b);

  DecomposedMatrix decomposed_interpolated =
      InterpolateMatrixDecompositions(decomposed_a, decomposed_b, progress);

  return RecomposeMatrix(decomposed_interpolated);
}

}  // namespace math
}  // namespace cobalt
