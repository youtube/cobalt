// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SKIA_SKIA_SURFACELESS_GL_RENDERER_H_
#define UI_OZONE_DEMO_SKIA_SKIA_SURFACELESS_GL_RENDERER_H_

#include <memory>

#include "ui/gfx/geometry/rect.h"
#include "ui/gl/presenter.h"
#include "ui/ozone/demo/skia/skia_gl_renderer.h"

namespace ui {
class OverlayCandidatesOzone;
class PlatformWindowSurface;

class SurfacelessSkiaGlRenderer : public SkiaGlRenderer {
 public:
  SurfacelessSkiaGlRenderer(
      gfx::AcceleratedWidget widget,
      std::unique_ptr<PlatformWindowSurface> window_surface,
      const scoped_refptr<gl::GLSurface>& offscreen_surface,
      const scoped_refptr<gl::Presenter>& presenter,
      const gfx::Size& size);

  SurfacelessSkiaGlRenderer(const SurfacelessSkiaGlRenderer&) = delete;
  SurfacelessSkiaGlRenderer& operator=(const SurfacelessSkiaGlRenderer&) =
      delete;

  ~SurfacelessSkiaGlRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  // SkiaGlRenderer:
  void RenderFrame() override;
  void PostRenderFrameTask(gfx::SwapCompletionResult result) override;

  class BufferWrapper;

  std::unique_ptr<BufferWrapper> buffers_[2];

  std::unique_ptr<BufferWrapper> overlay_buffer_[2];
  bool disable_primary_plane_ = false;
  gfx::Rect primary_plane_rect_;

  std::unique_ptr<OverlayCandidatesOzone> overlay_checker_;
  scoped_refptr<gl::Presenter> presenter_;

  int back_buffer_ = 0;

  base::WeakPtrFactory<SurfacelessSkiaGlRenderer> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SKIA_SKIA_SURFACELESS_GL_RENDERER_H_
