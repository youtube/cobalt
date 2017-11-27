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
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor_draw_state.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Implements skia-specific cache recording and playback surface management
// in order to provide skia-support for the render tree surface caching system.
class SurfaceCacheDelegate : public common::SurfaceCache::Delegate {
 private:
  class CachedSurface : public common::SurfaceCache::CachedSurface {
   public:
    CachedSurface(SkSurface* surface, const math::Vector2dF& output_post_scale,
                  const math::Vector2dF& output_pre_translate,
                  const math::Rect& output_bounds)
        : surface_(surface),
          image_(surface_ ? surface_->newImageSnapshot() : NULL),
          output_post_scale_(output_post_scale),
          output_pre_translate_(output_pre_translate),
          output_bounds_(output_bounds) {}
    ~CachedSurface() override {}

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

  // Creating a ScopedContext object declares a render tree visitor context,
  // and so there should be one ScopedContext per render tree visitor.  Since
  // it is typical to recursively create sub render tree visitors, this class
  // is designed to be instantiated multiple times on the same
  // SurfaceCacheDelegate object.
  class ScopedContext {
   public:
    ScopedContext(SurfaceCacheDelegate* delegate,
                  RenderTreeNodeVisitorDrawState* draw_state)
        : delegate_(delegate), old_draw_state_(delegate_->draw_state_) {
      // Setup our new draw state (including render target) to the provided one.
      delegate_->draw_state_ = draw_state;

      // A new canvas may mean a new total matrix, so inform our delegate to
      // refresh its scale.
      delegate_->UpdateCanvasScale();
    }
    ~ScopedContext() {
      // Restore the draw state to the old setting.
      delegate_->draw_state_ = old_draw_state_;

      if (delegate_->draw_state_) {
        delegate_->UpdateCanvasScale();
      }
    }

   private:
    SurfaceCacheDelegate* delegate_;
    RenderTreeNodeVisitorDrawState* old_draw_state_;
  };

  SurfaceCacheDelegate(
      const CreateSkSurfaceFunction& create_sk_surface_function,
      const math::Size& max_surface_size);

  // Must be called every time our client changes the current scale on us, e.g.
  // when visiting a MatrixTransformNode.
  void UpdateCanvasScale();

  void ApplySurface(common::SurfaceCache::CachedSurface* surface) override;
  void StartRecording(const math::RectF& local_bounds) override;
  common::SurfaceCache::CachedSurface* EndRecording() override;
  void ReleaseSurface(common::SurfaceCache::CachedSurface* surface) override;
  math::Size GetRenderSize(const math::SizeF& local_size) override;
  math::Size MaximumSurfaceSize() override;

 private:
  // Defines a set of data that is relevant only while recording.
  struct RecordingData {
    RecordingData(const RenderTreeNodeVisitorDrawState& original_draw_state,
                  const common::OffscreenRenderCoordinateMapping& coord_mapping,
                  SkSurface* surface)
        : original_draw_state(original_draw_state),
          coord_mapping(coord_mapping),
          surface(surface) {}

    // The original draw state (including render target/canvas) that the
    // recorded draw calls would have otherwise targeted if we were not
    // recording.
    RenderTreeNodeVisitorDrawState original_draw_state;

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

  // The maximum size of an SkSurface.
  math::Size max_surface_size_;

  // The current draw state (including the SkCanvas) that is being targeted,
  // whether its the main onscreen surface or a cached surface.
  RenderTreeNodeVisitorDrawState* draw_state_;

  // If we are currently recording, this will contain all of the data relevant
  // to that recording.
  base::optional<RecordingData> recording_data_;

  // The current scale of draw_state_->render_target->getTotalMatrix().  This
  // lets us quickly check if the scale of a render node changes from the last
  // time we observed it, which may happen if an animation is targeting a
  // MatrixTransformNode render tree node.
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
