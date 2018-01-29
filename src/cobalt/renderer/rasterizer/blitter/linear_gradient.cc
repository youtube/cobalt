// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/linear_gradient.h"

#include <algorithm>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/renderer/rasterizer/blitter/cobalt_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "cobalt/renderer/rasterizer/blitter/skia_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/skia/conversions.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

#if SB_HAS(BLITTER)

namespace {

using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::ColorStopList;
using cobalt::render_tree::LinearGradientBrush;
using cobalt::renderer::rasterizer::blitter::RenderState;
using cobalt::renderer::rasterizer::blitter::SkiaToBlitterPixelFormat;
using cobalt::renderer::rasterizer::blitter::RectFToBlitterRect;
using cobalt::renderer::rasterizer::blitter::LinearGradientCache;
using cobalt::renderer::rasterizer::skia::SkiaColorStops;

int GetPixelOffsetInBytes(const SkImageInfo& image_info,
                          const SbBlitterPixelData& pixel_data) {
  if (image_info.width() == 1)
    return SbBlitterGetPixelDataPitchInBytes(pixel_data);

  SbBlitterPixelDataFormat pixel_format =
      SkiaToBlitterPixelFormat(image_info.colorType());
  return SbBlitterBytesPerPixelForFormat(pixel_format);
}

void ColorStopToPixelData(SbBlitterPixelDataFormat pixel_format,
                          const ColorRGBA& color_stop, uint8_t* pixel_data_out,
                          uint8_t* pixel_data_out_end) {
  DCHECK(pixel_data_out_end >=
         (pixel_data_out + SbBlitterBytesPerPixelForFormat(pixel_format)));
  uint8_t final_b(color_stop.rgb8_b());
  uint8_t final_g(color_stop.rgb8_g());
  uint8_t final_r(color_stop.rgb8_r());
  uint8_t final_a(color_stop.rgb8_a());

  switch (pixel_format) {
    case kSbBlitterPixelDataFormatARGB8:
      pixel_data_out[0] = final_a;
      pixel_data_out[1] = final_r;
      pixel_data_out[2] = final_g;
      pixel_data_out[3] = final_b;
      break;
    case kSbBlitterPixelDataFormatBGRA8:
      pixel_data_out[0] = final_b;
      pixel_data_out[1] = final_g;
      pixel_data_out[2] = final_r;
      pixel_data_out[3] = final_a;
      break;
    case kSbBlitterPixelDataFormatRGBA8:
      pixel_data_out[0] = final_r;
      pixel_data_out[1] = final_g;
      pixel_data_out[2] = final_b;
      pixel_data_out[3] = final_a;
      break;
    default:
      NOTREACHED() << "Unknown or unsupported pixel format." << pixel_format;
      break;
  }
}

// This function uses Skia to render a gradient into the pixel data pointed
// at pixel_data_begin.  This function is intended as a fallback in case when
// RenderSimpleGradient is unable to render the gradient.
void RenderComplexLinearGradient(const LinearGradientBrush& brush,
                                 const ColorStopList& color_stops, int width,
                                 int height, const SkImageInfo& image_info,
                                 uint8_t* pixel_data_begin,
                                 int pitch_in_bytes) {
  SkBitmap bitmap;
  bitmap.installPixels(image_info, pixel_data_begin, pitch_in_bytes);
  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  SkPoint points[2];
  points[0].fX = 0;
  points[0].fY = 0;
  points[1].fX = 0;
  points[1].fY = 0;

  // The -1 offset is easiest to think about in a 2 pixel case:
  // If width = 2, then the ending coordinate should be 1.
  if (brush.IsHorizontal()) {
    if (brush.dest().x() < brush.source().x()) {
      points[0].fX = width - 1;  // Right to Left.
    } else {
      points[1].fX = width - 1;  // Left to Right.
    }
  } else {
    if (brush.dest().y() < brush.source().y()) {
      points[0].fY = height - 1;  // High to Low.
    } else {
      points[1].fY = height - 1;  // Low to High.
    }
  }

  SkiaColorStops skia_color_stops(color_stops);
  sk_sp<SkShader> shader(SkGradientShader::MakeLinear(
      points, skia_color_stops.colors.data(), skia_color_stops.positions.data(),
      skia_color_stops.size(), SkShader::kClamp_TileMode,
      SkGradientShader::kInterpolateColorsInPremul_Flag, NULL));
  SkPaint paint;
  paint.setShader(shader);
  canvas.drawPaint(paint);
}

// This function tries to render a "simple" gradient into the pixel data pointed
// to in the range [pixel_data_begin, pixel_data_end).  Simple gradients here
// are defined as has following properties:
//   1.  Vertical or horizontal gradient.
//   2.  Two color stops only -- one at position 0, and one at position 1.
//
// Having these conditions greatly simplifies the logic, and allows us to avoid
// the overhead of dealing with Skia.
//
// Returns: true if it could successfully render, false if optimal conditions
// are not met.  If false is returned, nothing is written to the pixel data
// buffer.
bool RenderSimpleGradient(const LinearGradientBrush& brush,
                          const ColorStopList& color_stops, int width,
                          int height, SbBlitterPixelDataFormat pixel_format,
                          uint8_t* pixel_data_begin, uint8_t* pixel_data_end,
                          int pixel_offset_in_bytes) {
  if (color_stops.size() != 2) {
    return false;
  }

  const float small_value(1E-6);
  if ((color_stops[0].position < -small_value) ||
      (color_stops[0].position > small_value)) {
    return false;
  }
  if ((color_stops[1].position < 1 - small_value) ||
      (color_stops[1].position > 1 + small_value)) {
    return false;
  }

  const int number_pixels = width * height;

  if ((number_pixels < 2) || (width < 0) || (height < 0)) {
    return false;
  }

  ColorRGBA start_color = color_stops[0].color;
  ColorRGBA end_color = color_stops[1].color;

  // Swap start and end colors if the gradient direction is right to left or
  // down to up.
  if (brush.IsHorizontal()) {
    if (brush.dest().x() < brush.source().x()) {
      std::swap(start_color, end_color);
    }
  } else {
    if (brush.dest().y() < brush.source().y()) {
      std::swap(start_color, end_color);
    }
  }

  ColorRGBA color_step((end_color - start_color) / (number_pixels - 1));
  // We add the color_stop * 0.5f to match pixel positioning in software Skia.
  ColorRGBA current_color(start_color + color_step * 0.5f);
  ColorStopToPixelData(pixel_format, current_color, pixel_data_begin,
                       pixel_data_end);

  uint8_t* second_pixel = pixel_data_begin + pixel_offset_in_bytes;
  uint8_t* last_pixel =
      (pixel_data_begin + (number_pixels - 1) * pixel_offset_in_bytes);
  for (uint8_t* current_pixel = second_pixel; current_pixel != last_pixel;
       current_pixel += pixel_offset_in_bytes) {
    current_color += color_step;
    ColorStopToPixelData(pixel_format, current_color, current_pixel,
                         pixel_data_end);
  }

  ColorStopToPixelData(pixel_format, end_color, last_pixel, pixel_data_end);

  return true;
}

void RenderOptimizedLinearGradient(SbBlitterDevice device,
                                   SbBlitterContext context,
                                   const RenderState& render_state,
                                   cobalt::math::RectF rect,
                                   const LinearGradientBrush& brush,
                                   LinearGradientCache* linear_gradient_cache) {
  DCHECK(linear_gradient_cache);
  if ((rect.width() == 0) && (rect.height() == 0)) return;

  DCHECK(brush.IsVertical() || brush.IsHorizontal());

  SbBlitterSurface surface = kSbBlitterInvalidSurface;
  if (linear_gradient_cache) {
    surface = linear_gradient_cache->Get(brush);
  }

  // The main strategy here is to create a 1D image, and then calculate
  // the gradient values in software.  If the gradient is simple, this can be
  // done with optimized function (RenderSimpleGradient), which avoids calling
  // Skia (and thus is faster).  Otherwise, we call RenderComplexLinearGradient,
  // which uses Skia.

  // Once a gradient is created, a SbBlitterSurface is created and a rectangle
  // is blitted using the blitter API.
  int width = brush.IsHorizontal() ?
              std::abs(brush.dest().x() - brush.source().x()) : 1;
  int height = brush.IsVertical() ?
              std::abs(brush.dest().y() - brush.source().y()) : 1;

  if (SbBlitterIsSurfaceValid(surface) == false) {
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(width, height);

    SbBlitterPixelDataFormat pixel_format =
        SkiaToBlitterPixelFormat(image_info.colorType());

    if (!SbBlitterIsPixelFormatSupportedByPixelData(device, pixel_format)) {
      NOTREACHED() << "Pixel Format is not supported by Pixel Data";
      return;
    }

    SbBlitterPixelData pixel_data =
        SbBlitterCreatePixelData(device, width, height, pixel_format);

    if (SbBlitterIsPixelDataValid(pixel_data)) {
      int bytes_per_pixel = GetPixelOffsetInBytes(image_info, pixel_data);
      int pitch_in_bytes = SbBlitterGetPixelDataPitchInBytes(pixel_data);

      uint8_t* pixel_data_begin =
          static_cast<uint8_t*>(SbBlitterGetPixelDataPointer(pixel_data));
      uint8_t* pixel_data_end = pixel_data_begin + pitch_in_bytes * height;

      const ColorStopList& color_stops(brush.color_stops());

      int pixel_offset_in_bytes =
          (width == 1) ? pitch_in_bytes : bytes_per_pixel;

      // Try rendering without Skia first.
      bool render_successful = RenderSimpleGradient(
          brush, color_stops, width, height, pixel_format, pixel_data_begin,
          pixel_data_end, pixel_offset_in_bytes);
      if (!render_successful) {
        RenderComplexLinearGradient(brush, color_stops, width, height,
                                    image_info, pixel_data_begin,
                                    pitch_in_bytes);
      }

      surface = SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
      linear_gradient_cache->Put(brush, surface);
    }
  }

  if (surface) {
    bool need_blending = false;
    for (ColorStopList::const_iterator it = brush.color_stops().begin();
         it != brush.color_stops().end(); ++it) {
      if (it->color.HasAlpha()) {
        need_blending = true;
        break;
      }
    }

    SbBlitterSetBlending(context, need_blending);
    SbBlitterSetModulateBlitsWithColor(context, false);
    cobalt::math::Rect transformed_rect = cobalt::math::Rect::RoundFromRectF(
        render_state.transform.TransformRect(rect));

    // It may be the case that the linear gradient is larger than the rect.
    SbBlitterRect source_rect;
    if (height == 1) {
      int left = rect.x() - std::min(brush.source().x(), brush.dest().x());
      source_rect = SbBlitterMakeRect(left, 0, rect.width(), 1);
    } else {
      int top = rect.y() - std::min(brush.source().y(), brush.dest().y());
      source_rect = SbBlitterMakeRect(0, top, 1, rect.height());
    }

    SbBlitterRect dest_rect =
        SbBlitterMakeRect(transformed_rect.x(), transformed_rect.y(),
                          transformed_rect.width(), transformed_rect.height());
    SbBlitterBlitRectToRect(context, surface, source_rect, dest_rect);
  }
}

void RenderColoredRect(SbBlitterDevice device, SbBlitterContext context,
                       const RenderState& render_state, const ColorRGBA& color,
                       const cobalt::math::RectF& rect) {
  SbBlitterSetBlending(context, color.a() < 1.0f);
  SbBlitterSetColor(context, SbBlitterColorFromRGBA(
      color.rgb8_r(), color.rgb8_g(), color.rgb8_b(), color.rgb8_a()));
  SbBlitterFillRect(context, RectFToBlitterRect(
      render_state.transform.TransformRect(rect)));
}

}  // namespace

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

using render_tree::LinearGradientBrush;

bool RenderLinearGradient(SbBlitterDevice device, SbBlitterContext context,
                          const RenderState& render_state,
                          const render_tree::RectNode& rect_node,
                          LinearGradientCache* linear_gradient_cache) {
  DCHECK(rect_node.data().background_brush);
  const LinearGradientBrush* const linear_gradient_brush =
      base::polymorphic_downcast<LinearGradientBrush*>(
          rect_node.data().background_brush.get());
  if (!linear_gradient_brush) return false;

  // Currently, only vertical and horizontal gradients are accelerated.
  math::RectF content_rect = rect_node.data().rect;
  if (linear_gradient_brush->IsVertical()) {
    // Render solid-colored rect(s) to fill any gaps between the content_rect
    // and the gradient.
    float top = linear_gradient_brush->source().y();
    float bottom = linear_gradient_brush->dest().y();
    ColorRGBA top_color = linear_gradient_brush->color_stops().front().color;
    ColorRGBA bottom_color = linear_gradient_brush->color_stops().back().color;
    if (top > bottom) {
      std::swap(top, bottom);
      std::swap(top_color, bottom_color);
    }

    if (top > content_rect.y()) {
      float gap = top - content_rect.y();
      RenderColoredRect(device, context, render_state, top_color,
          math::RectF(content_rect.x(), content_rect.y(), content_rect.width(),
                      gap));
      content_rect.set_y(top);
      content_rect.set_height(content_rect.height() - gap);
    }
    if (bottom < content_rect.bottom()) {
      float gap = content_rect.bottom() - bottom;
      RenderColoredRect(device, context, render_state, bottom_color,
          math::RectF(content_rect.x(), bottom, content_rect.width(), gap));
      content_rect.set_height(content_rect.height() - gap);
    }
  } else if (linear_gradient_brush->IsHorizontal()) {
    // Render solid-colored rect(s) to fill any gaps between the content_rect
    // and the gradient.
    float left = linear_gradient_brush->source().x();
    float right = linear_gradient_brush->dest().x();
    ColorRGBA left_color = linear_gradient_brush->color_stops().front().color;
    ColorRGBA right_color = linear_gradient_brush->color_stops().back().color;
    if (left > right) {
      std::swap(left, right);
      std::swap(left_color, right_color);
    }

    if (left > content_rect.x()) {
      float gap = left - content_rect.x();
      RenderColoredRect(device, context, render_state, left_color,
          math::RectF(content_rect.x(), content_rect.y(), gap,
                      content_rect.height()));
      content_rect.set_x(left);
      content_rect.set_width(content_rect.width() - gap);
    }
    if (right < content_rect.right()) {
      float gap = content_rect.right() - right;
      RenderColoredRect(device, context, render_state, right_color,
          math::RectF(right, content_rect.y(), gap, content_rect.height()));
      content_rect.set_width(content_rect.width() - gap);
    }
  } else {
    // Angled gradients are not supported by the optimized path.
    return false;
  }

  RenderOptimizedLinearGradient(device, context, render_state,
                                content_rect, *linear_gradient_brush,
                                linear_gradient_cache);
  return true;
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
