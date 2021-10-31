// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkColorType RenderTreeSurfaceFormatToSkia(
    render_tree::PixelFormat render_tree_format) {
  switch (render_tree_format) {
    case render_tree::kPixelFormatRGBA8:
      return kRGBA_8888_SkColorType;
    case render_tree::kPixelFormatBGRA8:
      return kBGRA_8888_SkColorType;
    case render_tree::kPixelFormatY8:
      return kAlpha_8_SkColorType;
    case render_tree::kPixelFormatU8:
      return kAlpha_8_SkColorType;
    case render_tree::kPixelFormatV8:
      return kAlpha_8_SkColorType;
    case render_tree::kPixelFormatUV8:
      return kRGBA_8888_SkColorType;
    default:
      DLOG(FATAL) << "Unknown render tree pixel format!";
      return kUnknown_SkColorType;
  }
}

SkAlphaType RenderTreeAlphaFormatToSkia(
    render_tree::AlphaFormat render_tree_format) {
  switch (render_tree_format) {
    case render_tree::kAlphaFormatPremultiplied:
      return kPremul_SkAlphaType;
    case render_tree::kAlphaFormatUnpremultiplied:
      return kUnpremul_SkAlphaType;
    case render_tree::kAlphaFormatOpaque:
      return kOpaque_SkAlphaType;
    default: DLOG(FATAL) << "Unknown render tree alpha format!";
  }
  return kUnpremul_SkAlphaType;
}

SkFontStyle CobaltFontStyleToSkFontStyle(render_tree::FontStyle style) {
  SkFontStyle::Slant slant = style.slant == render_tree::FontStyle::kItalicSlant
                                 ? SkFontStyle::kItalic_Slant
                                 : SkFontStyle::kUpright_Slant;

  return SkFontStyle(style.weight, SkFontStyle::kNormal_Width, slant);
}

SkRect CobaltRectFToSkiaRect(const math::RectF& rect) {
  return SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

SkMatrix CobaltMatrixToSkia(const math::Matrix3F& cobalt_matrix) {
  // Shorten the variable name.
  const math::Matrix3F& cm = cobalt_matrix;

  SkMatrix skia_matrix;
  skia_matrix.setAll(cm.Get(0, 0), cm.Get(0, 1), cm.Get(0, 2), cm.Get(1, 0),
                     cm.Get(1, 1), cm.Get(1, 2), cm.Get(2, 0), cm.Get(2, 1),
                     cm.Get(2, 2));
  return skia_matrix;
}

math::Matrix3F SkiaMatrixToCobalt(const SkMatrix& skia_matrix) {
  return math::Matrix3F::FromValues(
      skia_matrix.get(0), skia_matrix.get(1), skia_matrix.get(2),
      skia_matrix.get(3), skia_matrix.get(4), skia_matrix.get(5),
      skia_matrix.get(6), skia_matrix.get(7), skia_matrix.get(8));
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
