// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_

#include "base/callback.h"
#include "base/optional.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/rounded_corners.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Base type to rasterize various objects via GL commands.
class DrawObject {
 public:
  typedef base::Optional<render_tree::RoundedCorners> OptionalRoundedCorners;

  // Callback to get a scratch "1D" texture of the given |size|. If such a
  // request was previously fulfilled for the relevant render tree node, then
  // the cached |texture| and |region| will be returned, and |is_new| will be
  // false; this is a hint as to whether the texture region needs to be
  // initialized. If the request succeeded, then |texture| will point to a 2D
  // texture whose |region| is a row meeting the size requirement; otherwise,
  // |texture| will be nullptr.
  struct TextureInfo {
    const backend::TextureEGL* texture;
    math::RectF region;
    bool is_new;
  };
  typedef base::Callback<void(float size, TextureInfo* out_texture_info)>
      GetScratchTextureFunction;

  // Structure containing the common attributes for all DrawObjects.
  struct BaseState {
    BaseState();
    BaseState(const BaseState& other);

    math::Matrix3F transform;
    math::Rect scissor;

    // |rounded_scissor_rect| is only relevant when |rounded_scissor_corners|
    // exists.
    math::RectF rounded_scissor_rect;
    OptionalRoundedCorners rounded_scissor_corners;

    float opacity;
  };

  virtual ~DrawObject() {}

  // Certain draw objects can be merged with others to reduce the number of
  // draw calls issued. If TryMerge returns true, then |other| can be discarded.
  base::TypeId GetMergeTypeId() const { return merge_type_; }
  virtual bool TryMerge(DrawObject* other) { return false; }

  // This stage is used to update the vertex buffer for the rasterize
  // stage. Vertex data is handled by the GraphicsState to minimize the number
  // of vertex buffers needed. Once this stage is executed, the rasterizer will
  // then notify the GraphicsState to send all vertex data from all draw
  // objects to the GPU.
  virtual void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state, ShaderProgramManager* program_manager) = 0;

  // This stage is responsible for issuing the GPU commands to do the actual
  // rendering.
  virtual void ExecuteRasterize(GraphicsState* graphics_state,
                                ShaderProgramManager* program_manager) = 0;

  // Return a TypeId that can be used to sort draw objects in order to minimize
  // state changes. This may be the class' TypeId, or the TypeId of the shader
  // program which the class uses.
  virtual base::TypeId GetTypeId() const = 0;

 protected:
  // Structures describing vertex data for rendering rounded rectangles.
  struct RCorner {
    // Constructor to transform a RCorner value into a format which the
    // shader function IsOutsideRCorner() expects. This expresses the current
    // vertex position as a scaled offset relevant for the corner, and provides
    // scalars to assist in calculating the antialiased edge. For more details,
    // see function_is_outside_rcorner.inc.
    RCorner(const float (&position)[2], const RCorner& init);
    RCorner() {}
    float x, y;
    float rx, ry;
  };
  struct RRectAttributes {
    math::RectF bounds;  // The region in which to use the rcorner data.
    RCorner rcorner;
  };

  DrawObject();
  explicit DrawObject(const BaseState& base_state);

  // Remove scale from the transform, and return the scale vector.
  math::Vector2dF RemoveScaleFromTransform();

  // Utility function to get the render color for the blend modes that will
  // be used. These modes expect alpha to be pre-multiplied.
  static render_tree::ColorRGBA GetDrawColor(
      const render_tree::ColorRGBA& color) {
    return render_tree::ColorRGBA(color.r() * color.a(), color.g() * color.a(),
                                  color.b() * color.a(), color.a());
  }

  // Return a uint32_t suitable to be transferred as 4 unsigned bytes
  // representing color to a GL shader.
  static uint32_t GetGLRGBA(float r, float g, float b, float a);
  static uint32_t GetGLRGBA(const render_tree::ColorRGBA& color) {
    return GetGLRGBA(color.r(), color.g(), color.b(), color.a());
  }

  // Get the vertex attributes to use to draw the given rounded rect. Each
  // corner uses a different attribute. These RCorner values must be transformed
  // before being passed to the shader. (See RCorner constructor.)
  static void GetRRectAttributes(const math::RectF& bounds, math::RectF rect,
                                 render_tree::RoundedCorners corners,
                                 RRectAttributes (&out_attributes)[4]);

  // Get the vertex attributes to draw the given rounded rect excluding the
  // inscribed rect. These RCorner values must be transformed before being
  // passed to the shader. (See RCorner constructor.)
  static void GetRRectAttributes(const math::RectF& bounds, math::RectF rect,
                                 render_tree::RoundedCorners corners,
                                 RRectAttributes (&out_attributes)[8]);

  BaseState base_state_;

  // Provide type information for use with TryMerge. Only DrawObjects that may
  // be merged need to set this.
  base::TypeId merge_type_;

 private:
  // Return the RCorner values for the given rounded rect, and the normalized
  // rect and corner values used.
  static void GetRCornerValues(math::RectF* rect,
                               render_tree::RoundedCorners* corners,
                               RRectAttributes out_rcorners[4]);
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_
