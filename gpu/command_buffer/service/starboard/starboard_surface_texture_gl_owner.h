// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef GPU_COMMAND_BUFFER_SERVICE_STARBOARD_STARBOARD_SURFACE_TEXTURE_GL_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_STARBOARD_STARBOARD_SURFACE_TEXTURE_GL_OWNER_H_

#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/gpu_export.h"
#include "ui/gl/android/surface_texture.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace media {
class StarboardRendererWrapper;
}  // namespace media

namespace gpu {

// TODO(borongchen): add class comment
class GPU_GLES2_EXPORT StarboardSurfaceTextureGLOwner : public TextureOwner {
 public:
  StarboardSurfaceTextureGLOwner(const StarboardSurfaceTextureGLOwner&) =
      delete;
  StarboardSurfaceTextureGLOwner& operator=(
      const StarboardSurfaceTextureGLOwner&) = delete;

  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  void SetFrameAvailableCallback(
      const base::RepeatingClosure& frame_available_cb) override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void EnsureTexImageBound(GLuint service_id) override;
  void ReleaseBackBuffers() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;
  bool GetCodedSizeAndVisibleRect(gfx::Size rotated_visible_size,
                                  gfx::Size* coded_size,
                                  gfx::Rect* visible_rect) override;

  void RunWhenBufferIsAvailable(base::OnceClosure callback) override;

 protected:
  void ReleaseResources() override;

 private:
  friend class media::StarboardRendererWrapper;

  StarboardSurfaceTextureGLOwner(
      std::unique_ptr<AbstractTextureAndroid> texture,
      scoped_refptr<SharedContextState> context_state);
  ~StarboardSurfaceTextureGLOwner() override;

  // The context and surface that were used to create |surface_texture_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STARBOARD_STARBOARD_SURFACE_TEXTURE_GL_OWNER_H_
