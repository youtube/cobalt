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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_COBALT_SKIA_TYPE_CONVERSIONS_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_COBALT_SKIA_TYPE_CONVERSIONS_H_

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/resource_provider.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkColorType RenderTreeSurfaceFormatToSkia(
    render_tree::PixelFormat render_tree_format);
SkAlphaType RenderTreeAlphaFormatToSkia(
    render_tree::AlphaFormat render_tree_format);

SkFontStyle CobaltFontStyleToSkFontStyle(render_tree::FontStyle style);

SkRect CobaltRectFToSkiaRect(const math::RectF& rect);

SkMatrix CobaltMatrixToSkia(const math::Matrix3F& cobalt_matrix);
math::Matrix3F SkiaMatrixToCobalt(const SkMatrix& skia_matrix);

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_COBALT_SKIA_TYPE_CONVERSIONS_H_
