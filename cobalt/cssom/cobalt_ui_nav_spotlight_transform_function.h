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

#ifndef COBALT_CSSOM_COBALT_UI_NAV_SPOTLIGHT_TRANSFORM_FUNCTION_H_
#define COBALT_CSSOM_COBALT_UI_NAV_SPOTLIGHT_TRANSFORM_FUNCTION_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/transform_function.h"

namespace cobalt {
namespace cssom {

class TransformFunctionVisitor;

// This Cobalt-specific transform function for hybrid navigation tracks
// the direction in which focus is moving. This can be used to provide
// feedback about user input that is too small to result in a focus
// change. The transform function queries the closest hybrid navigation
// focus item (i.e. this element or an ancestor element with tabindex
// set appropriately). If the closest hybrid navigation focus item does
// not have focus, or the platform does not provide focus movement
// information, then this transform function will evaluate to the zero
// scale. Otherwise the transform will evaluate to a translation ranging
// from -50% to +50% in the X and Y directions.
class CobaltUiNavSpotlightTransformFunction : public TransformFunction {
 public:
  CobaltUiNavSpotlightTransformFunction();

  void Accept(TransformFunctionVisitor* visitor) const override;

  std::string ToString() const override;

  math::Matrix3F ToMatrix(const math::SizeF& used_size,
      const scoped_refptr<ui_navigation::NavItem>& used_ui_nav_focus)
      const override;

  bool operator==(const CobaltUiNavSpotlightTransformFunction& other) const {
    return true;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(CobaltUiNavSpotlightTransformFunction);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COBALT_UI_NAV_SPOTLIGHT_TRANSFORM_FUNCTION_H_
