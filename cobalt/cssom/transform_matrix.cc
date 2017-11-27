// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/transform_matrix.h"

#include "base/logging.h"
#include "cobalt/cssom/matrix_function.h"
#include "cobalt/cssom/rotate_function.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/transform_function.h"
#include "cobalt/cssom/transform_function_visitor.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/math/matrix_interpolation.h"
#include "cobalt/math/transform_2d.h"

namespace cobalt {
namespace cssom {

TransformMatrix::TransformMatrix() : matrix_(math::Matrix3F::Identity()) {}

TransformMatrix::TransformMatrix(const math::Matrix3F& matrix)
    : matrix_(matrix) {}

math::Matrix3F TransformMatrix::ToMatrix3F(const math::SizeF& used_size) const {
  math::Matrix3F result(matrix_);
  for (int i = 0; i < 2; ++i) {
    result.Set(i, 2,
               width_percentage_translation_[i] * used_size.width() +
                   height_percentage_translation_[i] * used_size.height() +
                   matrix_.Get(i, 2));
  }
  return result;
}

TransformMatrix InterpolateTransformMatrices(const TransformMatrix& a,
                                             const TransformMatrix& b,
                                             float progress) {
  // By interpolating the matrix and percentage translate vectors separately,
  // we achieve the same results as if we had decomposed the matrix with
  // the multi-component translate values embedded within it.
  math::Matrix3F interpolated_matrix(
      math::InterpolateMatrices(a.matrix_, b.matrix_, progress));

  math::Vector2dF interpolated_width_percentage_translation =
      a.width_percentage_translation_ +
      ScaleVector2d(
          b.width_percentage_translation_ - a.width_percentage_translation_,
          progress);

  math::Vector2dF interpolated_height_percentage_translation =
      a.height_percentage_translation_ +
      ScaleVector2d(
          b.height_percentage_translation_ - a.height_percentage_translation_,
          progress);

  return TransformMatrix(interpolated_matrix,
                         interpolated_width_percentage_translation,
                         interpolated_height_percentage_translation);
}

namespace {

// The PostMultiplyMatrixVisitor will post-multiply the passed in matrix
// with a matrix representing the transformation that is visited.
class PostMultiplyMatrixVisitor : public TransformFunctionVisitor {
 public:
  explicit PostMultiplyMatrixVisitor(
      math::Matrix3F* matrix, math::Vector2dF* width_percentage_translation,
      math::Vector2dF* height_percentage_translation)
      : matrix_(matrix),
        width_percentage_translation_(width_percentage_translation),
        height_percentage_translation_(height_percentage_translation) {}

  void VisitMatrix(const MatrixFunction* matrix_function) override;
  void VisitRotate(const RotateFunction* rotate_function) override;
  void VisitScale(const ScaleFunction* scale_function) override;
  void VisitTranslate(const TranslateFunction* translate_function) override;

 private:
  math::Matrix3F* matrix_;
  math::Vector2dF* width_percentage_translation_;
  math::Vector2dF* height_percentage_translation_;
};

void PostMultiplyMatrixVisitor::VisitMatrix(
    const MatrixFunction* matrix_function) {
  *matrix_ = *matrix_ * matrix_function->value();
}

void PostMultiplyMatrixVisitor::VisitRotate(
    const RotateFunction* rotate_function) {
  // Since RotateMatrix()'s parameter is interpreted as counter-clockwise, we
  // must negate our clockwise angle before passing it in.
  *matrix_ = *matrix_ *
             math::RotateMatrix(-rotate_function->clockwise_angle_in_radians());
}

void PostMultiplyMatrixVisitor::VisitScale(
    const ScaleFunction* scale_function) {
  *matrix_ = *matrix_ * math::ScaleMatrix(scale_function->x_factor(),
                                          scale_function->y_factor());
}

void PostMultiplyMatrixVisitor::VisitTranslate(
    const TranslateFunction* translate_function) {
  if (translate_function->length_component_in_pixels() == 0.0f &&
      translate_function->percentage_component() == 0.0f) {
    // If we are translating by 0, there is nothing to be done here, this
    // is the identity matrix.
    return;
  }

  if (translate_function->axis() == TranslateFunction::kZAxis) {
    NOTIMPLEMENTED() << "translateZ is currently a noop in Cobalt.";
    return;
  }

  int axis_index =
      translate_function->axis() == TranslateFunction::kXAxis ? 0 : 1;

  math::Vector2dF* target_percentage_translation =
      axis_index == 0 ? width_percentage_translation_
                      : height_percentage_translation_;

  // We manually apply the translation matrix transformation here, so that
  // we can have it correctly apply to the percentage components as well.
  for (int i = 0; i < 2; ++i) {
    float scale = matrix_->Get(i, axis_index);
    (*matrix_)(i, 2) +=
        translate_function->length_component_in_pixels() * scale;
    (*target_percentage_translation)[i] +=
        translate_function->percentage_component() * scale;
  }
}

}  // namespace

void TransformMatrix::PostMultiplyByTransform(
    const TransformFunction* function) {
  PostMultiplyMatrixVisitor visitor(&matrix_, &width_percentage_translation_,
                                    &height_percentage_translation_);
  function->Accept(&visitor);
}

}  // namespace cssom
}  // namespace cobalt
