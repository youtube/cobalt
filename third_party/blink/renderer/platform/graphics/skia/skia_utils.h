/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// All of the functions in this file should move to new homes and this file
// should be deleted.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SKIA_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SKIA_UTILS_H_

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {
class LayoutRect;

/**** constants ****/

enum {
  // Firefox limits width/height to 32767 pixels, but slows down dramatically
  // before it reaches that limit. We limit by area instead, giving us larger
  // maximum dimensions, in exchange for a smaller maximum canvas size.
  kMaxCanvasArea = 32768 * 8192,  // Maximum canvas area in CSS pixels

  // In Skia, we will also limit width/height to 65535.
  kMaxSkiaDim = 65535  // Maximum width/height in CSS pixels.
};

bool PLATFORM_EXPORT IsValidImageSize(const gfx::Size&);

SkBlendMode PLATFORM_EXPORT
    WebCoreCompositeToSkiaComposite(CompositeOperator,
                                    BlendMode = BlendMode::kNormal);
SkBlendMode PLATFORM_EXPORT WebCoreBlendModeToSkBlendMode(BlendMode);

std::pair<CompositeOperator, BlendMode> PLATFORM_EXPORT
CompositeAndBlendOpsFromSkBlendMode(SkBlendMode sk_blend_mode);

// Multiply a color's alpha channel by an additional alpha factor where
// alpha is in the range [0, 1].
SkColor PLATFORM_EXPORT ScaleAlpha(SkColor, float);

bool PLATFORM_EXPORT
ApproximatelyEqualSkColorSpaces(sk_sp<SkColorSpace> src_color_space,
                                sk_sp<SkColorSpace> dst_color_space);

SkRect PLATFORM_EXPORT LayoutRectToSkRect(const blink::LayoutRect& rect);

// Skia has problems when passed infinite, etc floats, filter them to 0.
inline SkScalar WebCoreFloatToSkScalar(float f) {
  return SkFloatToScalar(std::isfinite(f) ? f : 0);
}

inline SkScalar WebCoreDoubleToSkScalar(double d) {
  return SkDoubleToScalar(std::isfinite(d) ? d : 0);
}

inline bool WebCoreFloatNearlyEqual(float a, float b) {
  return SkScalarNearlyEqual(WebCoreFloatToSkScalar(a),
                             WebCoreFloatToSkScalar(b));
}

inline SkPathFillType WebCoreWindRuleToSkFillType(WindRule rule) {
  return static_cast<SkPathFillType>(rule);
}

inline WindRule SkFillTypeToWindRule(SkPathFillType fill_type) {
  switch (fill_type) {
    case SkPathFillType::kWinding:
    case SkPathFillType::kEvenOdd:
      return static_cast<WindRule>(fill_type);
    default:
      NOTREACHED();
      break;
  }
  return RULE_NONZERO;
}

inline SkPoint FloatPointToSkPoint(const gfx::PointF& point) {
  return SkPoint::Make(WebCoreFloatToSkScalar(point.x()),
                       WebCoreFloatToSkScalar(point.y()));
}

SkMatrix PLATFORM_EXPORT AffineTransformToSkMatrix(const AffineTransform&);
SkM44 PLATFORM_EXPORT AffineTransformToSkM44(const AffineTransform&);

bool NearlyIntegral(float value);

InterpolationQuality ComputeInterpolationQuality(float src_width,
                                                 float src_height,
                                                 float dest_width,
                                                 float dest_height,
                                                 bool is_data_complete = true);

// Technically, this is driven by the CSS/Canvas2D specs and unrelated to Skia.
// It should probably live in the CSS layer, but the notion of a "blur radius"
// leaks into platform/graphics currently (ideally we should only deal with
// sigma at this level).
// TODO(fmalita): find a better home for this helper.
inline float BlurRadiusToStdDev(float radius) {
  DCHECK_GE(radius, 0);

  // Per spec, sigma is exactly half the blur radius:
  // https://www.w3.org/TR/css-backgrounds-3/#shadow-blur
  // https://html.spec.whatwg.org/C/#when-shadows-are-drawn
  return radius * 0.5f;
}

void PLATFORM_EXPORT DrawPlatformFocusRing(const SkRRect&,
                                           cc::PaintCanvas*,
                                           SkColor,
                                           float width);
void PLATFORM_EXPORT DrawPlatformFocusRing(const SkPath&,
                                           cc::PaintCanvas*,
                                           SkColor,
                                           float width,
                                           float corner_radius);

inline SkCanvas::SrcRectConstraint WebCoreClampingModeToSkiaRectConstraint(
    Image::ImageClampingMode clamp_mode) {
  return clamp_mode == Image::kClampImageToSourceRect
             ? SkCanvas::kStrict_SrcRectConstraint
             : SkCanvas::kFast_SrcRectConstraint;
}

// Attempts to allocate an SkData on the PartitionAlloc buffer partition.
// If this fails (e.g. due to low memory), returns a null sk_sp<SkData> instead.
// Otherwise, the returned buffer is guaranteed to be zero-filled.
PLATFORM_EXPORT sk_sp<SkData> TryAllocateSkData(size_t size);

// Skia's smart pointer APIs are preferable over their legacy raw pointer
// counterparts.
//
// General guidelines
//
// When receiving ref counted objects from Skia:
//
//   1) Use sk_sp-based Skia factories if available (e.g. SkShader::MakeFoo()
//      instead of SkShader::CreateFoo()).
//   2) Use sk_sp<T> locals for all objects.
//
// When passing ref counted objects to Skia:
//
//   1) Use sk_sp-based Skia APIs when available (e.g.
//      SkPaint::setShader(sk_sp<SkShader>) instead of
//      SkPaint::setShader(SkShader*)).
//   2) If object ownership is being passed to Skia, use std::move(sk_sp<T>).
//
// Example (creating a SkShader and setting it on SkPaint):
//
// a) ownership transferred
//
//     // using Skia smart pointer locals
//     sk_sp<SkShader> shader = SkShader::MakeFoo(...);
//     paint.setShader(std::move(shader));
//
//     // using no locals
//     paint.setShader(SkShader::MakeFoo(...));
//
// b) shared ownership
//
//     sk_sp<SkShader> shader = SkShader::MakeFoo(...);
//     paint.setShader(shader);

}  // namespace blink

namespace WTF {

// We define CrossThreadCopier<SKBitMap> here because we cannot include skia
// headers in platform/wtf.
template <>
struct CrossThreadCopier<SkBitmap> {
  STATIC_ONLY(CrossThreadCopier);

  using Type = SkBitmap;
  static SkBitmap Copy(const SkBitmap& bitmap) {
    CHECK(bitmap.isImmutable() || bitmap.isNull())
        << "Only immutable bitmaps can be transferred.";
    return bitmap;
  }
  static SkBitmap Copy(SkBitmap&& bitmap) {
    CHECK(bitmap.isImmutable() || bitmap.isNull())
        << "Only immutable bitmaps can be transferred.";
    return std::move(bitmap);
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_SKIA_UTILS_H_
