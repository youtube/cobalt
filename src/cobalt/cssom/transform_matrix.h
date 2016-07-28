/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_TRANSFORM_MATRIX_H_
#define COBALT_CSSOM_TRANSFORM_MATRIX_H_

#include "cobalt/cssom/transform_function.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace cssom {

// Matrices that represent CSS transforms are special since a transform can
// be specified in terms of length or percentage translate values, and these
// values can then be combined (e.g. through animations like CSS Transitions)
// into a matrix.  Therefore, to support arbitrary combinations of length and
// percentage translate values, we must maintain this information in a special
// matrix where each translation value (e.g. x and y) is actually a triplet:
//
// ( offset in pixels,
//   offset as a percentage of used width,
//   offset as a percentage of used height )
//
// The reason why we must maintain a percentage of height component for the
// x translation unit (and vice-versa with width and y axis) is because one may
// specify a transform as follows:
//
//   transform: rotate(90deg) translateX(50%)
//
// In this case, since a percentage value is given for translateX, it is
// interpreted as a percentage of width, however it is then rotated 90 degrees,
// and so we now have a translation along the y axis that is expressed as
// a percentage of the used width.
//
class TransformMatrix {
 public:
  TransformMatrix();
  explicit TransformMatrix(const math::Matrix3F& matrix);

  // Collapses this TransformMatrix to a normal Matrix3F.  In order to do this,
  // percentage values must be resolved, and so a used_size must be provided.
  math::Matrix3F ToMatrix3F(const math::SizeF& used_size) const;

  // Returns true if the percentage data for this matrix is zero (e.g. this
  // matrix does not depend on a used size).
  bool percentage_fields_zero() const {
    return width_percentage_translation_.IsZero() &&
           height_percentage_translation_.IsZero();
  }

  // Modifies this matrix by post-multiplying by the matrix associated with
  // the specified transformation.
  void PostMultiplyByTransform(const TransformFunction* function);

  // Returns a matrix representing the current transformation, where the
  // translation represented by the matrix is in units of pixels.  This method
  // is mostly useful for testing purposes only.
  const math::Matrix3F& offset_matrix() const { return matrix_; }

  // Returns the width percentage units of translation expressed by this matrix.
  // This method is mostly useful for testing purposes only.
  const math::Vector2dF& width_percentage_translation() const {
    return width_percentage_translation_;
  }
  // Returns the height percentage units of translation expressed by this
  // matrix.  This method is mostly useful for testing purposes only.
  const math::Vector2dF& height_percentage_translation() const {
    return height_percentage_translation_;
  }

  bool operator==(const TransformMatrix& rhs) const {
    return matrix_ == rhs.matrix_ &&
           width_percentage_translation_ == rhs.width_percentage_translation_ &&
           height_percentage_translation_ == rhs.height_percentage_translation_;
  }

 private:
  TransformMatrix(const math::Matrix3F& matrix,
                  const math::Vector2dF& width_percentage_translation,
                  const math::Vector2dF& height_percentage_translation)
      : matrix_(matrix),
        width_percentage_translation_(width_percentage_translation),
        height_percentage_translation_(height_percentage_translation) {}


  // Translation of pixel units is represented in the matrix (along with all
  // non-translate transformations).
  math::Matrix3F matrix_;

  // Translation of width/height percentage units is represented in the
  // following two vectors.
  math::Vector2dF width_percentage_translation_;
  math::Vector2dF height_percentage_translation_;

  friend TransformMatrix InterpolateTransformMatrices(const TransformMatrix& a,
                                                      const TransformMatrix& b,
                                                      float progress);
};

// Interpolates from one TransformMatrix to another.
TransformMatrix InterpolateTransformMatrices(const TransformMatrix& a,
                                             const TransformMatrix& b,
                                             float progress);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_TRANSFORM_MATRIX_H_
