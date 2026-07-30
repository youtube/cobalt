// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ZOOM_MIGRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ZOOM_MIGRATION_H_

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class ComputedStyle;

inline gfx::PointF NoopWillBeScalePoint(const gfx::PointF& point, float zoom) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled()
             ? gfx::ScalePoint(point, zoom)
             : point;
}

inline gfx::PointF NoopWillBeInvScalePoint(gfx::PointF point, float zoom) {
  if (RuntimeEnabledFeatures::SvgNewZoomEnabled()) {
    point.InvScale(zoom);
  }
  return point;
}

inline float NoopWillBeScaleScalar(float value, float zoom) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled() ? value * zoom : value;
}

inline float NoopWillBeInvScaleScalar(float value, float zoom) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled() ? value / zoom : value;
}

inline gfx::PointF ScalePointWillBeNoop(const gfx::PointF& point, float zoom) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled()
             ? point
             : gfx::ScalePoint(point, zoom);
}

inline gfx::RectF ScaleRectWillBeNoop(const gfx::RectF& rect, float zoom) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled()
             ? rect
             : gfx::ScaleRect(rect, zoom);
}

inline gfx::RectF NoopWillBeInvScaleRect(gfx::RectF rect, float zoom) {
  if (RuntimeEnabledFeatures::SvgNewZoomEnabled()) {
    rect.InvScale(zoom);
  }
  return rect;
}

inline gfx::RectF NoopWillBeScaleRect(const gfx::RectF& rect, float zoom) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled()
             ? gfx::ScaleRect(rect, zoom)
             : rect;
}

inline LayoutObject::OutlineInfo SvgOutlineInfo(const ComputedStyle& style) {
  if (RuntimeEnabledFeatures::SvgNewZoomEnabled()) {
    return LayoutObject::OutlineInfo::GetFromStyle(style);
  }
  return LayoutObject::OutlineInfo::GetUnzoomedFromStyle(style);
}

inline float NoZoomWillBeSvgObjectZoom(const ComputedStyle& style) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled() ? style.EffectiveZoom()
                                                     : 1;
}

inline float SvgObjectZoomWillBeNoZoom(const ComputedStyle& style) {
  return RuntimeEnabledFeatures::SvgNewZoomEnabled() ? 1
                                                     : style.EffectiveZoom();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ZOOM_MIGRATION_H_
