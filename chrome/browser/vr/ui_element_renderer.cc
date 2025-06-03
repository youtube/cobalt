// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_element_renderer.h"

#include <math.h>
#include <algorithm>
#include <string>

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "chrome/browser/vr/renderers/base_renderer.h"
#include "chrome/browser/vr/renderers/grid_renderer.h"
#include "chrome/browser/vr/renderers/radial_gradient_quad_renderer.h"
#include "chrome/browser/vr/renderers/texture_copy_renderer.h"
#include "chrome/browser/vr/renderers/textured_quad_renderer.h"
#include "chrome/browser/vr/renderers/transparent_quad_renderer.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

UiElementRenderer::UiElementRenderer() : UiElementRenderer(true) {}

UiElementRenderer::UiElementRenderer(bool use_gl) {
  if (!use_gl)
    return;
  Init();
  BaseQuadRenderer::CreateBuffers();
  TexturedQuadRenderer::CreateBuffers();
}

UiElementRenderer::~UiElementRenderer() = default;

void UiElementRenderer::Init() {
  transparent_quad_renderer_ = std::make_unique<TransparentQuadRenderer>();
  textured_quad_renderer_ = std::make_unique<TexturedQuadRenderer>();
  radial_gradient_quad_renderer_ =
      std::make_unique<RadialGradientQuadRenderer>();
  texture_copy_renderer_ = std::make_unique<TextureCopyRenderer>();
  gradient_grid_renderer_ = std::make_unique<GridRenderer>();
}

void UiElementRenderer::DrawTexturedQuad(
    int texture_data_handle,
    int overlay_texture_data_handle,
    const gfx::Transform& model_view_proj_matrix,
    const gfx::RectF& clip_rect,
    float opacity,
    const gfx::SizeF& element_size,
    float corner_radius,
    bool blend) {
  TRACE_EVENT0("gpu", "UiElementRenderer::DrawTexturedQuad");
  // TODO(vollick): handle drawing this degenerate situation crbug.com/768922
  if (corner_radius * 2.0 > element_size.width() ||
      corner_radius * 2.0 > element_size.height()) {
    return;
  }
  TexturedQuadRenderer* renderer = textured_quad_renderer_.get();
  if (!texture_data_handle && !overlay_texture_data_handle) {
    // TODO(https://crbug.com/913607): Per SkiaSurfaceProvider's helper method
    // GetTextureIDFromSurface, it appears as though texture_data_handle cannot
    // be null, and therefore this should be unused. However, in case another
    // path was missed, removal of the transparent_quad_renderer should be done
    // in its own change.
    // If we're blending, why are we even drawing a transparent quad?
    DCHECK(!blend);
    renderer = transparent_quad_renderer_.get();
  }
  FlushIfNecessary(renderer);
  renderer->AddQuad(texture_data_handle, overlay_texture_data_handle,
                    model_view_proj_matrix, clip_rect, opacity, element_size,
                    corner_radius, blend);
}

void UiElementRenderer::DrawRadialGradientQuad(
    const gfx::Transform& model_view_proj_matrix,
    const SkColor edge_color,
    const SkColor center_color,
    const gfx::RectF& clip_rect,
    float opacity,
    const gfx::SizeF& element_size,
    const CornerRadii& radii) {
  TRACE_EVENT0("gpu", "UiElementRenderer::DrawRadialGradientQuad");
  FlushIfNecessary(radial_gradient_quad_renderer_.get());
  radial_gradient_quad_renderer_->Draw(model_view_proj_matrix, edge_color,
                                       center_color, clip_rect, opacity,
                                       element_size, radii);
}

void UiElementRenderer::DrawGradientGridQuad(
    const gfx::Transform& model_view_proj_matrix,
    const SkColor grid_color,
    int gridline_count,
    float opacity) {
  TRACE_EVENT0("gpu", "UiElementRenderer::DrawGradientGridQuad");
  FlushIfNecessary(gradient_grid_renderer_.get());
  gradient_grid_renderer_->Draw(model_view_proj_matrix, grid_color,
                                gridline_count, opacity);
}

void UiElementRenderer::DrawTextureCopy(int texture_data_handle,
                                        const float (&uv_transform)[16],
                                        float xborder,
                                        float yborder) {
  FlushIfNecessary(texture_copy_renderer_.get());
  texture_copy_renderer_->Draw(texture_data_handle, uv_transform, xborder,
                               yborder);
}

void UiElementRenderer::Flush() {
  textured_quad_renderer_->Flush();
  last_renderer_ = nullptr;
}

void UiElementRenderer::FlushIfNecessary(BaseRenderer* renderer) {
  if (last_renderer_ && renderer != last_renderer_) {
    last_renderer_->Flush();
  }
  last_renderer_ = renderer;
}

}  // namespace vr
