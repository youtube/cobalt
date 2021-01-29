// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_COBALT_UI_NAV_FOCUS_TRANSFORM_FUNCTION_H_
#define COBALT_CSSOM_COBALT_UI_NAV_FOCUS_TRANSFORM_FUNCTION_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

class TransformFunctionVisitor;

// This Cobalt-specific transform function for hybrid navigation tracks
// interaction with the navigation item when it is focused. This function
// queries the closest hybrid navigation focus item (i.e. this element or
// an ancestor element with tabindex set appropriately). If that navigation
// item does not have focus, or the system does not provide interaction
// animations, then this transform function will evaluate to identity. If
// no ancestor is a hybrid navigation focus item, then the currently-focused
// item (which may be in a different subtree) is queried.
class CobaltUiNavFocusTransformFunction : public TransformFunction {
 public:
  // |progress_to_identity| is used to support transitions to or from CSS
  // transform none. The value must be in the range of [0,1] with a value of 1
  // resulting in this transform returning the identity matrix. DOM elements
  // will only ever use progress_to_identity == 0, but intermediate animation
  // frames may use other values.
  CobaltUiNavFocusTransformFunction(float x_translation_scale,
                                    float y_translation_scale,
                                    float progress_to_identity = 0.0f);

  void Accept(TransformFunctionVisitor* visitor) const override;

  float x_translation_scale() const { return x_translation_scale_; }
  float y_translation_scale() const { return y_translation_scale_; }
  float progress_to_identity() const { return progress_to_identity_; }

  std::string ToString() const override;

  math::Matrix3F ToMatrix(const math::SizeF& used_size,
                          const scoped_refptr<ui_navigation::NavItem>&
                              used_ui_nav_focus) const override;

  bool operator==(const CobaltUiNavFocusTransformFunction& other) const {
    return x_translation_scale_ == other.x_translation_scale_ &&
           y_translation_scale_ == other.y_translation_scale_ &&
           progress_to_identity_ == other.progress_to_identity_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(CobaltUiNavFocusTransformFunction);

 private:
  const float x_translation_scale_;
  const float y_translation_scale_;
  const float progress_to_identity_;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COBALT_UI_NAV_FOCUS_TRANSFORM_FUNCTION_H_
