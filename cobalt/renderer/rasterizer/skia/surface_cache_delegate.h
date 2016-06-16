/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SURFACE_CACHE_DELEGATE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SURFACE_CACHE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/base/console_commands.h"
#endif
#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"
#include "cobalt/renderer/rasterizer/common/surface_cache.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Implements skia-specific cache recording and playback surface management
// in order to provide skia-support for the render tree surface caching system.
class SurfaceCacheDelegate : public SurfaceCache::Delegate {
 private:
  class CachedSurface : public SurfaceCache::CachedSurface {
   public:
    CachedSurface(SkSurface* surface, const math::Vector2dF& output_post_scale,
                  const math::Vector2dF& output_pre_translate,
                  const math::Rect& output_bounds)
        : surface_(surface),
          image_(surface_ ? surface_->newImageSnapshot() : NULL),
          output_post_scale_(output_post_scale),
          output_pre_translate_(output_pre_translate),
          output_bounds_(output_bounds) {}
    ~CachedSurface() OVERRIDE {}

    SkImage* image() const { return image_.get(); }
    const math::Vector2dF& output_post_scale() const {
      return output_post_scale_;
    }
    const math::Vector2dF& output_pre_translate() const {
      return output_pre_translate_;
    }
    const math::Rect& output_bounds() const { return output_bounds_; }

   private:
    // The actual skia surface representing the cached render tree.
    SkAutoTUnref<SkSurface> surface_;
    // For convenience, we save the image snapshot associated with |surface_|
    // above.
    SkAutoTUnref<SkImage> image_;
    // When applying |surface_| to another render target, the current
    // transformation should be post-multiplied by |output_post_scale_|, and
    // pre-multiplied by |output_pre_translate_|.
    math::Vector2dF output_post_scale_;
    math::Vector2dF output_pre_translate_;
    // The rectangle coordinates into which |surface_| should be rendered.
    math::Rect output_bounds_;
  };

 public:
  typedef base::Callback<SkSurface*(const math::Size&)> CreateSkSurfaceFunction;
  typedef base::Callback<void(SkCanvas*)> SetCanvasFunction;

  // Creating a ScopedContext object declares a render tree visitor context,
  // and so there should be one ScopedContext per render tree visitor.  Since
  // it is typical to recursively create sub render tree visitors, this class
  // is designed to be instantiated multiple times on the same
  // SurfaceCacheDelegate object.
  class ScopedContext {
   public:
    ScopedContext(SurfaceCacheDelegate* delegate, SkCanvas* canvas,
                  const SetCanvasFunction& set_canvas_function)
        : delegate_(delegate),
          old_canvas_(delegate_->canvas_),
          old_set_canvas_function_(delegate_->set_canvas_function_) {
      // Setup our new canvas to the provided one.  Remember a new method
      // to set the canvas for the current rendering context in case we need
      // to start recording on a offscreen surface.
      delegate_->canvas_ = canvas;
      delegate_->set_canvas_function_ = set_canvas_function;

      // A new canvas may mean a new total matrix, so inform our delegate to
      // refresh its scale.
      delegate_->UpdateCanvasScale();
    }
    ~ScopedContext() {
      // Restore the canvas to the old setting.
      delegate_->set_canvas_function_ = old_set_canvas_function_;
      delegate_->canvas_ = old_canvas_;

      if (delegate_->canvas_) {
        delegate_->UpdateCanvasScale();
      }
    }

   private:
    SurfaceCacheDelegate* delegate_;
    SkCanvas* old_canvas_;
    SetCanvasFunction old_set_canvas_function_;
  };

  SurfaceCacheDelegate(
      const CreateSkSurfaceFunction& create_sk_surface_function,
      const math::Size& max_surface_size);

  // Must be called every time our client changes the current scale on us, e.g.
  // when visiting a MatrixTransformNode.
  void UpdateCanvasScale();

  void ApplySurface(SurfaceCache::CachedSurface* surface) OVERRIDE;
  void StartRecording(const math::RectF& local_bounds) OVERRIDE;
  SurfaceCache::CachedSurface* EndRecording() OVERRIDE;
  void ReleaseSurface(SurfaceCache::CachedSurface* surface) OVERRIDE;
  math::Size GetRenderSize(const math::SizeF& local_size) OVERRIDE;
  math::Size MaximumSurfaceSize() OVERRIDE;

 private:
  // Defines a set of data that is relevant only while recording.
  struct RecordingData {
    RecordingData(SkCanvas* original_canvas,
                  const common::OffscreenRenderCoordinateMapping& coord_mapping,
                  SkSurface* surface)
        : original_canvas(original_canvas),
          coord_mapping(coord_mapping),
          surface(surface) {}

    // The original canvas that the recorded draw calls would have otherwise
    // targeted if we were not recording.
    SkCanvas* original_canvas;

    // Information about how to map the offscreen cached surface to the main
    // render target.
    common::OffscreenRenderCoordinateMapping coord_mapping;

    // The offscreen cached surface that we are recording to.
    SkSurface* surface;
  };

#if defined(ENABLE_DEBUG_CONSOLE)
  void OnToggleCacheHighlights(const std::string& message);
#endif

  // A function that will return a fresh SkSurface object when we need to cache
  // something new.
  CreateSkSurfaceFunction create_sk_surface_function_;

  // A function that can be used to change the current canvas in order to
  // redirect Skia rasterization commands to an offscreen cached surface.
  SetCanvasFunction set_canvas_function_;

  // The maximum size of an SkSurface.
  math::Size max_surface_size_;

  // The current canvas that is being targeted, whether its the main onscreen
  // surface or a cached surface.
  SkCanvas* canvas_;

  // If we are currently recording, this will contain all of the data relevant
  // to that recording.
  base::optional<RecordingData> recording_data_;

  // The current scale of canvas_->getTotalMatrix().  This lets us quickly check
  // if the scale of a render node changes from the last time we observed it,
  // which may happen if an animation is targeting a MatrixTransformNode render
  // tree node.
  math::Vector2dF scale_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Debug command to toggle cache highlights to help visualize which nodes
  // are being cached.
  bool toggle_cache_highlights_;
  base::ConsoleCommandManager::CommandHandler
      toggle_cache_highlights_command_handler_;
#endif
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SURFACE_CACHE_DELEGATE_H_
