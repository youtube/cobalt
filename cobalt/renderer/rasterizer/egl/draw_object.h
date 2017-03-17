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
    float depth;
    float opacity;
  };

  virtual ~DrawObject() {}

  // This stage is executed before vertex data is calculated. This allows
  // processing which may impact vertex data (such as vertex or texture
  // coordinates).
  //
  // The most common use is to allocate space in an offscreen atlas, then
  // render to it using a different rasterizer. Since the location in the
  // offscreen atlas impacts the texture coordinates used for onscreen
  // rendering, offscreen rendering to a texture atlas needs to happen here.
  //
  // If this stage ever needs to use a vertex buffer of its own in order
  // to perform offscreen rendering, then a new stage, similar to the current
  // "ExecuteUpdateVertexBuffer" stage, should be added and executed before
  // this stage.
  virtual void ExecutePreVertexBuffer(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) {}

  // This stage is used to update the vertex buffer for a later stage to use.
  // Vertex data is handled by the GraphicsState to minimize the number of
  // vertex buffers needed. Once this stage is executed, the rasterizer will
  // then notify the GraphicsState to send all vertex data from all draw
  // objects to the GPU.
  virtual void ExecuteUpdateVertexBuffer(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) = 0;

  // This stage is used to render onscreen. Although it can be used to
  // rasterize to any number of render targets, it is best to use a different
  // stage for offscreen rendering in order to minimize the cost of switching
  // render targets.
  virtual void ExecuteRasterizeNormal(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) = 0;

 protected:
  explicit DrawObject(const BaseState& base_state);

  // Return a uint32_t suitable to be transferred as 4 unsigned bytes
  // representing color to a GL shader.
  static uint32_t GetGLRGBA(float r, float g, float b, float a);

  BaseState base_state_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_OBJECT_H_
