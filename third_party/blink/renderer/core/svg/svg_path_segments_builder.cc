// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_path_segments_builder.h"

#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_svg_path_segment.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"

namespace blink {

SVGPathSegmentsBuilder::SVGPathSegmentsBuilder() = default;

// The switch mirrors `SVGPathStringBuilder::EmitSegment()` (modulo the
// command character vs. values mapping).
void SVGPathSegmentsBuilder::EmitSegment(const PathSegmentData& segment) {
  Vector<float> values;
  switch (segment.command) {
    case kPathSegClosePath:
      break;
    case kPathSegMoveToAbs:
    case kPathSegMoveToRel:
    case kPathSegLineToAbs:
    case kPathSegLineToRel:
    case kPathSegCurveToQuadraticSmoothAbs:
    case kPathSegCurveToQuadraticSmoothRel:
      values = {segment.X(), segment.Y()};
      break;
    case kPathSegLineToHorizontalAbs:
    case kPathSegLineToHorizontalRel:
      values = {segment.X()};
      break;
    case kPathSegLineToVerticalAbs:
    case kPathSegLineToVerticalRel:
      values = {segment.Y()};
      break;
    case kPathSegCurveToCubicAbs:
    case kPathSegCurveToCubicRel:
      values = {segment.X1(), segment.Y1(), segment.X2(),
                segment.Y2(), segment.X(),  segment.Y()};
      break;
    case kPathSegCurveToCubicSmoothAbs:
    case kPathSegCurveToCubicSmoothRel:
      values = {segment.X2(), segment.Y2(), segment.X(), segment.Y()};
      break;
    case kPathSegCurveToQuadraticAbs:
    case kPathSegCurveToQuadraticRel:
      values = {segment.X1(), segment.Y1(), segment.X(), segment.Y()};
      break;
    case kPathSegArcAbs:
    case kPathSegArcRel:
      values = {segment.ArcRadiusX(),
                segment.ArcRadiusY(),
                segment.ArcAngle(),
                segment.LargeArcFlag() ? 1.0f : 0.0f,
                segment.SweepFlag() ? 1.0f : 0.0f,
                segment.X(),
                segment.Y()};
      break;
    case kPathSegUnknown:
      NOTREACHED();
  }

  auto* path_segment = SVGPathSegment::Create();
  path_segment->setType(
      String(base::span_from_ref(kPathSegmentCharacter[segment.command])));
  path_segment->setValues(std::move(values));
  result_.push_back(path_segment);
}

HeapVector<Member<SVGPathSegment>> SVGPathSegmentsBuilder::Finalize() {
  return std::move(result_);
}

}  // namespace blink
