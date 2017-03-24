// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/offscreen_target_manager.h"

#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrRenderTarget.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
int32_t NextPowerOf2(int32_t num) {
  // Return the smallest power of 2 that is greater than or equal to num.
  // This flips on all bits <= num, then num+1 will be the next power of 2.
  --num;
  num |= num >> 1;
  num |= num >> 2;
  num |= num >> 4;
  num |= num >> 8;
  num |= num >> 16;
  return num + 1;
}

bool IsPowerOf2(int32_t num) {
  return (num & (num - 1)) == 0;
}
}  // namespace

OffscreenTargetManager::OffscreenTargetManager(
    backend::GraphicsContextEGL* graphics_context, GrContext* skia_context)
    : graphics_context_(graphics_context),
      skia_context_(skia_context),
      offscreen_atlas_size_(0, 0),
      offscreen_target_size_mask_(0, 0) {
}

void OffscreenTargetManager::Update(const math::Size& frame_size) {
  // Set initial characteristics for offscreen target handling.
  if (offscreen_atlas_size_.IsEmpty()) {
    if (frame_size.width() >= 64 && frame_size.height() >= 64) {
      offscreen_atlas_size_.SetSize(
          NextPowerOf2(frame_size.width() / 16),
          NextPowerOf2(frame_size.height() / 16));
      offscreen_target_size_mask_.SetSize(
          NextPowerOf2(frame_size.width() / 64) - 1,
          NextPowerOf2(frame_size.height() / 64) - 1);
    } else {
      offscreen_atlas_size_.SetSize(16, 16);
      offscreen_target_size_mask_.SetSize(0, 0);
    }
  }

  // Keep only the largest offscreen target atlas.
  while (offscreen_atlases_.size() > 1) {
    offscreen_atlases_.erase(offscreen_atlases_.begin());
  }

  if (!offscreen_atlases_.empty()) {
    offscreen_atlases_.back()->allocator.Reset();
    offscreen_atlases_.back()->skia_surface->getCanvas()->clear(
        SK_ColorTRANSPARENT);
  }
}

bool OffscreenTargetManager::GetCachedOffscreenTarget(
    const render_tree::Node* node, const math::SizeF& size,
    backend::FramebufferEGL** out_framebuffer, SkCanvas** out_skia_canvas,
    math::RectF* out_target_rect) {
  return false;
}

void OffscreenTargetManager::AllocateOffscreenTarget(
    const render_tree::Node* node, const math::SizeF& size,
    backend::FramebufferEGL** out_framebuffer, SkCanvas** out_skia_canvas,
    math::RectF* out_target_rect) {
  // Get an offscreen target for rendering. Align up the requested target size
  // to improve usage of the atlas (since more requests will have the same
  // aligned width or height).
  DCHECK(IsPowerOf2(offscreen_target_size_mask_.width() + 1));
  DCHECK(IsPowerOf2(offscreen_target_size_mask_.height() + 1));
  math::Size target_size(
      (static_cast<int>(size.width() + 0.5f) +
          offscreen_target_size_mask_.width()) &
          ~offscreen_target_size_mask_.width(),
      (static_cast<int>(size.height() + 0.5f) +
          offscreen_target_size_mask_.height()) &
          ~offscreen_target_size_mask_.height());
  math::Rect target_rect(0, 0, 0, 0);
  OffscreenAtlas* atlas = NULL;

  // See if there's room in the most recently created offscreen target atlas.
  // Don't search any other atlases since we want to quickly find an offscreen
  // atlas size large enough to hold all offscreen targets needed in a frame.
  if (!offscreen_atlases_.empty()) {
    atlas = offscreen_atlases_.back();
    target_rect = atlas->allocator.Allocate(target_size);
  }

  if (target_rect.IsEmpty()) {
    // Create a new offscreen atlas, bigger than the previous, so that
    // eventually only one offscreen atlas is needed per frame.
    bool grew = false;
    if (offscreen_atlas_size_.width() < target_size.width()) {
      offscreen_atlas_size_.set_width(NextPowerOf2(target_size.width()));
      grew = true;
    }
    if (offscreen_atlas_size_.height() < target_size.height()) {
      offscreen_atlas_size_.set_height(NextPowerOf2(target_size.height()));
      grew = true;
    }
    if (!grew) {
      // Grow the offscreen atlas while keeping it square-ish.
      if (offscreen_atlas_size_.width() <= offscreen_atlas_size_.height()) {
        offscreen_atlas_size_.set_width(offscreen_atlas_size_.width() * 2);
      } else {
        offscreen_atlas_size_.set_height(offscreen_atlas_size_.height() * 2);
      }
    }

    atlas = AddOffscreenAtlas(offscreen_atlas_size_);
    target_rect = atlas->allocator.Allocate(target_size);
  }
  DCHECK(!target_rect.IsEmpty());

  *out_framebuffer = atlas->framebuffer.get();
  *out_skia_canvas = atlas->skia_surface->getCanvas();
  *out_target_rect = target_rect;
}

OffscreenTargetManager::OffscreenAtlas*
    OffscreenTargetManager::AddOffscreenAtlas(const math::Size& size) {
  OffscreenAtlas* atlas = new OffscreenAtlas(offscreen_atlas_size_);
  offscreen_atlases_.push_back(atlas);

  // Create a new framebuffer.
  atlas->framebuffer.reset(new backend::FramebufferEGL(
      graphics_context_, offscreen_atlas_size_, GL_RGBA, GL_NONE));

  // Wrap the framebuffer as a skia surface.
  GrBackendRenderTargetDesc skia_desc;
  skia_desc.fWidth = offscreen_atlas_size_.width();
  skia_desc.fHeight = offscreen_atlas_size_.height();
  skia_desc.fConfig = kRGBA_8888_GrPixelConfig;
  skia_desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  skia_desc.fSampleCnt = 0;
  skia_desc.fStencilBits = 0;
  skia_desc.fRenderTargetHandle = atlas->framebuffer->gl_handle();

  SkAutoTUnref<GrRenderTarget> skia_render_target(
      skia_context_->wrapBackendRenderTarget(skia_desc));
  SkSurfaceProps skia_surface_props(
      SkSurfaceProps::kUseDistanceFieldFonts_Flag,
      SkSurfaceProps::kLegacyFontHost_InitType);
  atlas->skia_surface.reset(SkSurface::NewRenderTargetDirect(
      skia_render_target, &skia_surface_props));

  atlas->skia_surface->getCanvas()->clear(SK_ColorTRANSPARENT);

  return atlas;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
