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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_

#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Base type to rasterize various objects via GL commands.
class DrawObject {
 public:
  // Structure containing the common attributes for all DrawObjects.
  struct BaseState {
    BaseState();
    BaseState(const BaseState& other);

    math::Matrix3F transform;
    math::Rect scissor;
    float opacity;
  };

  virtual ~DrawObject() {}

  // This stage is used to update the vertex buffer for the rasterize
  // stage. Vertex data is handled by the GraphicsState to minimize the number
  // of vertex buffers needed. Once this stage is executed, the rasterizer will
  // then notify the GraphicsState to send all vertex data from all draw
  // objects to the GPU.
  virtual void ExecuteUpdateVertexBuffer(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) = 0;

  // This stage is responsible for issuing the GPU commands to do the actual
  // rendering.
  virtual void ExecuteRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) = 0;

  // Return a TypeId that can be used to sort draw objects in order to minimize
  // state changes. This may be the class' TypeId, or the TypeId of the shader
  // program which the class uses.
  virtual base::TypeId GetTypeId() const = 0;

 protected:
  DrawObject() {}
  explicit DrawObject(const BaseState& base_state);

  // Return a uint32_t suitable to be transferred as 4 unsigned bytes
  // representing color to a GL shader.
  static uint32_t GetGLRGBA(float r, float g, float b, float a);
  static uint32_t GetGLRGBA(const render_tree::ColorRGBA& color) {
    return GetGLRGBA(color.r(), color.g(), color.b(), color.a());
  }

  BaseState base_state_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_
