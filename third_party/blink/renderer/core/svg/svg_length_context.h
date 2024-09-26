/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_unit_types.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class SizeF;
class Vector2dF;
}  // namespace gfx

namespace blink {

class ComputedStyle;
class Element;
class LayoutObject;
class SVGElement;
class SVGLength;
class UnzoomedLength;

enum class SVGLengthMode { kWidth, kHeight, kOther };

class CORE_EXPORT SVGLengthContext {
  STACK_ALLOCATED();

 public:
  explicit SVGLengthContext(const SVGElement*);

  template <typename T>
  static gfx::RectF ResolveRectangle(const T* context,
                                     SVGUnitTypes::SVGUnitType type,
                                     const gfx::RectF& viewport) {
    return ResolveRectangle(
        context, type, viewport, *context->x()->CurrentValue(),
        *context->y()->CurrentValue(), *context->width()->CurrentValue(),
        *context->height()->CurrentValue());
  }

  static gfx::RectF ResolveRectangle(const SVGElement*,
                                     SVGUnitTypes::SVGUnitType,
                                     const gfx::RectF& viewport,
                                     const SVGLength& x,
                                     const SVGLength& y,
                                     const SVGLength& width,
                                     const SVGLength& height);
  gfx::Vector2dF ResolveLengthPair(const Length& x_length,
                                   const Length& y_length,
                                   const ComputedStyle&) const;

  float ConvertValueToUserUnits(float,
                                SVGLengthMode,
                                CSSPrimitiveValue::UnitType from_unit) const;
  float ConvertValueFromUserUnits(float,
                                  SVGLengthMode,
                                  CSSPrimitiveValue::UnitType to_unit) const;

  float ValueForLength(const UnzoomedLength&,
                       SVGLengthMode = SVGLengthMode::kOther) const;
  float ValueForLength(const Length&,
                       const ComputedStyle&,
                       SVGLengthMode = SVGLengthMode::kOther) const;
  static float ValueForLength(const Length&,
                              const ComputedStyle&,
                              float dimension);

  gfx::SizeF ResolveViewport() const;
  float ViewportDimension(SVGLengthMode) const;
  float ResolveValue(const CSSPrimitiveValue&, SVGLengthMode) const;

 private:
  float ValueForLength(const Length&, float zoom, SVGLengthMode) const;
  static float ValueForLength(const Length&, float zoom, float dimension);

  double ConvertValueToUserUnitsUnclamped(
      float value,
      SVGLengthMode mode,
      CSSPrimitiveValue::UnitType from_unit) const;

  const SVGElement* context_;
};

class SVGLengthConversionData : public CSSToLengthConversionData {
  STACK_ALLOCATED();

 public:
  SVGLengthConversionData(const Element& context, const ComputedStyle& style);
  explicit SVGLengthConversionData(const LayoutObject& object);

 private:
  CSSToLengthConversionData::Flags ignored_flags_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_CONTEXT_H_
