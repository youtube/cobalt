// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_CLEAR_FRAMEBUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_CLEAR_FRAMEBUFFER_H_

#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_gles2_export.h"

namespace gfx {
class Size;
}

namespace gpu {
namespace gles2 {
class GLES2Decoder;

class GPU_GLES2_EXPORT ClearFramebufferResourceManager {
 public:
  ClearFramebufferResourceManager(const gles2::GLES2Decoder* decoder);

  ClearFramebufferResourceManager(const ClearFramebufferResourceManager&) =
      delete;
  ClearFramebufferResourceManager& operator=(
      const ClearFramebufferResourceManager&) = delete;

  ~ClearFramebufferResourceManager();

  void Destroy();
  void ClearFramebuffer(const gles2::GLES2Decoder* decoder,
                        const gfx::Size& max_viewport_size,
                        GLbitfield mask,
                        GLfloat clear_color_red,
                        GLfloat clear_color_green,
                        GLfloat clear_color_blue,
                        GLfloat clear_color_alpha,
                        GLfloat clear_depth_value,
                        GLint clear_stencil_value);

 private:
  void Initialize(const gles2::GLES2Decoder* decoder);

  // The attributes used during invocation of the extension.
  static const GLuint kVertexPositionAttrib = 0;

  bool initialized_;
  GLuint program_;
  GLuint depth_handle_;
  GLuint color_handle_;
  GLuint buffer_id_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_CLEAR_FRAMEBUFFER_H_
