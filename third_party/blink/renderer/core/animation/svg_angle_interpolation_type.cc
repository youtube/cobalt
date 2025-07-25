// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_angle_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/svg/svg_angle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolationValue SVGAngleInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(0));
}

InterpolationValue SVGAngleInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (!To<SVGAngle>(svg_value).IsNumeric())
    return nullptr;
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(To<SVGAngle>(svg_value).Value()));
}

SVGPropertyBase* SVGAngleInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  double double_value = To<InterpolableNumber>(interpolable_value).Value();
  auto* result = MakeGarbageCollected<SVGAngle>();
  result->NewValueSpecifiedUnits(SVGAngle::kSvgAngletypeDeg, double_value);
  return result;
}

}  // namespace blink
