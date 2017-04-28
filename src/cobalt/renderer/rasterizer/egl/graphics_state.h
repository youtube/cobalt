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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_GRAPHICS_STATE_H_
#define COBALT_RENDERER_RASTERIZER_EGL_GRAPHICS_STATE_H_

#include <GLES2/gl2.h>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Class representing all the GL graphics states pertinent to rendering. This
// caches the state information and helps reduce redundant GL calls.
class GraphicsState {
 public:
  GraphicsState();
  ~GraphicsState();

  // Mark all cached states as dirty. GL commands to reset the state will be
  // executed on the next call to Clear or UseProgram.
  void SetDirty();

  // Specify the beginning of a new render frame.
  void BeginFrame();

  // Specify the end of the current render frame.
  void EndFrame();

  // Clear the color and depth buffers.
  void Clear();

  // Set the current shader program to be used.
  void UseProgram(GLuint program);

  // Set the viewport.
  void Viewport(int x, int y, int width, int height);
  const math::Rect& GetViewport() const { return viewport_; }

  // Set the scissor box.
  void Scissor(int x, int y, int width, int height);
  const math::Rect& GetScissor() const { return scissor_; }

  // Control blending state.
  // Default = disabled.
  void EnableBlend();
  void DisableBlend();

  // Control depth testing.
  // Default = enabled.
  void EnableDepthTest();
  void DisableDepthTest();

  // Control writing to the depth buffer.
  // Default = enabled.
  void EnableDepthWrite();
  void DisableDepthWrite();

  // Bind a texture to a given texture unit. Combines glActiveTexture and
  // glBindTexture.
  void ActiveBindTexture(GLenum texture_unit, GLenum target, GLuint texture);

  // Bind a texture to the specified texture unit and set its texture wrap
  // mode.
  void ActiveBindTexture(GLenum texture_unit, GLenum target, GLuint texture,
                         GLint texture_wrap_mode);

  // Set the clip adjustment to be used with vertex shaders. This transforms
  // the vertex coordinates from view space to clip space.
  void SetClipAdjustment(const math::Size& render_target_size);

  // Update the GPU with the current clip adjustment settings.
  void UpdateClipAdjustment(GLint handle);

  // Update the GPU with the specified transform matrix.
  void UpdateTransformMatrix(GLint handle, const math::Matrix3F& transform);

  // Reserve the specified number of bytes for vertex data in the upcoming
  // frame. This must be called outside of a render frame (i.e. before
  // BeginFrame / after EndFrame).
  void ReserveVertexData(size_t bytes);

  // Returns a client-side pointer to the specified number of bytes in the
  // vertex data buffer. The number of bytes allocated for any given frame
  // should be less than or equal to the number of bytes reserved. This must
  // be done after all calls to ReserveVertexData for a given render frame.
  uint8_t* AllocateVertexData(size_t bytes);

  // Update the GPU with the current contents of the vertex data buffer. This
  // should only be called once, after BeginFrame().
  void UpdateVertexData();

  // Specify vertex attribute data that the current program will use.
  // |client_pointer| should be within the range of addresses returned by
  // AllocateVertexData.
  void VertexAttribPointer(GLint index, GLint size, GLenum type,
      GLboolean normalized, GLsizei stride, const void* client_pointer);

  // Disable any vertex attrib arrays that the previous program used (via
  // VertexAttribPointer), but the current program does not.
  void VertexAttribFinish();

  // Return the farthest depth value.
  static float FarthestDepth();

  // Return the next closest depth value. Vertices using the output depth are
  // guaranteed to render on top of vertices using the incoming depth.
  static float NextClosestDepth(float depth);

 private:
  void Reset();

  math::Rect viewport_;
  math::Rect scissor_;
  math::Size render_target_size_;

  GLuint program_;
  GLuint array_buffer_handle_;
  GLenum texture_unit_;
  uint32_t enabled_vertex_attrib_array_mask_;
  uint32_t disable_vertex_attrib_array_mask_;
  float clip_adjustment_[4];
  bool clip_adjustment_dirty_;
  bool state_dirty_;
  bool blend_enabled_;
  bool depth_test_enabled_;
  bool depth_write_enabled_;

  static const int kNumTextureUnitsCached = 8;
  GLenum texunit_target_[kNumTextureUnitsCached];
  GLuint texunit_texture_[kNumTextureUnitsCached];

  static const int kNumFramesBuffered = 3;
  int frame_index_;

  static const size_t kVertexDataAlignment = 4;
  std::vector<uint8_t> vertex_data_buffer_;
  size_t vertex_data_reserved_;
  size_t vertex_data_allocated_;
  GLuint vertex_data_buffer_handle_[kNumFramesBuffered];
  bool vertex_data_buffer_updated_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_GRAPHICS_STATE_H_
