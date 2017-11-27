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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_SURFACE_CACHE_DELEGATE_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_SURFACE_CACHE_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/base/console_commands.h"
#endif
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"
#include "cobalt/renderer/rasterizer/common/surface_cache.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// Implements blitter-specific cache recording and playback surface management
// in order to provide blitter-support for the render tree surface caching
// system.
class SurfaceCacheDelegate : public common::SurfaceCache::Delegate {
 private:
  class CachedSurface : public common::SurfaceCache::CachedSurface {
   public:
    CachedSurface(SbBlitterSurface surface,
                  const math::Vector2dF& output_post_scale,
                  const math::Vector2dF& output_pre_translate,
                  const math::Rect& output_bounds)
        : surface_(surface),
          output_post_scale_(output_post_scale),
          output_pre_translate_(output_pre_translate),
          output_bounds_(output_bounds) {}
    ~CachedSurface() override { SbBlitterDestroySurface(surface_); }

    SbBlitterSurface surface() const { return surface_; }
    const math::Vector2dF& output_post_scale() const {
      return output_post_scale_;
    }
    const math::Vector2dF& output_pre_translate() const {
      return output_pre_translate_;
    }
    const math::Rect& output_bounds() const { return output_bounds_; }

   private:
    // The actual surface representing the cached render tree.
    SbBlitterSurface surface_;

    // When applying |surface_| to another render target, the current
    // transformation should be post-multiplied by |output_post_scale_|, and
    // pre-multiplied by |output_pre_translate_|.
    math::Vector2dF output_post_scale_;
    math::Vector2dF output_pre_translate_;
    // The rectangle coordinates into which |surface_| should be rendered.
    math::Rect output_bounds_;
  };

 public:
  typedef base::Callback<RenderState*(const RenderState&)>
      SetRenderStateFunction;

  // Creating a ScopedContext object declares a render tree visitor context,
  // and so there should be one ScopedContext per render tree visitor.  Since
  // it is typical to recursively create sub render tree visitors, this class
  // is designed to be instantiated multiple times on the same
  // SurfaceCacheDelegate object.
  class ScopedContext {
   public:
    ScopedContext(SurfaceCacheDelegate* delegate, RenderState* render_state,
                  const SetRenderStateFunction& set_render_state_function)
        : delegate_(delegate),
          old_render_state_(delegate_->render_state_),
          old_set_render_state_function_(
              delegate_->set_render_state_function_) {
      // Setup our new render target to the provided one.  Remember a new method
      // to set the render target for the current rendering context in case we
      // need to start recording on a offscreen surface.
      delegate_->render_state_ = render_state;
      delegate_->set_render_state_function_ = set_render_state_function;
    }
    ~ScopedContext() {
      // Restore the render target to the old setting.
      delegate_->set_render_state_function_ = old_set_render_state_function_;
      delegate_->render_state_ = old_render_state_;
    }

   private:
    SurfaceCacheDelegate* delegate_;
    RenderState* old_render_state_;
    SetRenderStateFunction old_set_render_state_function_;
  };

  SurfaceCacheDelegate(SbBlitterDevice device, SbBlitterContext context);

  void ApplySurface(common::SurfaceCache::CachedSurface* surface) override;
  void StartRecording(const math::RectF& local_bounds) override;
  common::SurfaceCache::CachedSurface* EndRecording() override;
  void ReleaseSurface(common::SurfaceCache::CachedSurface* surface) override;
  math::Size GetRenderSize(const math::SizeF& local_size) override;
  math::Size MaximumSurfaceSize() override;

 private:
  // Defines a set of data that is relevant only while recording.
  struct RecordingData {
    RecordingData(const RenderState& original_render_state,
                  const common::OffscreenRenderCoordinateMapping& coord_mapping,
                  SbBlitterSurface surface)
        : original_render_state(original_render_state),
          coord_mapping(coord_mapping),
          surface(surface) {}

    // The original render state that the recorded draw calls would have
    // otherwise targeted if we were not recording.
    RenderState original_render_state;

    // Information about how to map the offscreen cached surface to the main
    // render target.
    common::OffscreenRenderCoordinateMapping coord_mapping;

    // The offscreen cached surface that we are recording to.
    SbBlitterSurface surface;
  };

#if defined(ENABLE_DEBUG_CONSOLE)
  void OnToggleCacheHighlights(const std::string& message);
#endif

  SbBlitterDevice device_;
  SbBlitterContext context_;

  // A function that can be used to change the current render state in order to
  // affect subsequent blitter render tree node visitor rasterization commands
  // to an offscreen cached surface with a custom transform and clip stack.
  SetRenderStateFunction set_render_state_function_;

  // The current render state that is being used, whether it is cached surface
  // recording state (e.g. render to an offscreen surface) or the normal
  // render state currently in use by a render tree node visitor client.
  RenderState* render_state_;

  // If we are currently recording, this will contain all of the data relevant
  // to that recording.
  base::optional<RecordingData> recording_data_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Debug command to toggle cache highlights to help visualize which nodes
  // are being cached.
  bool toggle_cache_highlights_;
  base::ConsoleCommandManager::CommandHandler
      toggle_cache_highlights_command_handler_;
#endif
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_SURFACE_CACHE_DELEGATE_H_
