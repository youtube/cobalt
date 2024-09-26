// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_BASIC_SHAPE_INTERPOLATION_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_BASIC_SHAPE_INTERPOLATION_FUNCTIONS_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class BasicShape;
class CSSValue;
class CSSToLengthConversionData;

namespace basic_shape_interpolation_functions {

InterpolationValue MaybeConvertCSSValue(const CSSValue&);
CORE_EXPORT InterpolationValue MaybeConvertBasicShape(const BasicShape*,
                                                      double zoom);
std::unique_ptr<InterpolableValue> CreateNeutralValue(
    const NonInterpolableValue&);
CORE_EXPORT bool ShapesAreCompatible(const NonInterpolableValue&,
                                     const NonInterpolableValue&);
CORE_EXPORT scoped_refptr<BasicShape> CreateBasicShape(
    const InterpolableValue&,
    const NonInterpolableValue&,
    const CSSToLengthConversionData&);

}  // namespace basic_shape_interpolation_functions

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_BASIC_SHAPE_INTERPOLATION_FUNCTIONS_H_
