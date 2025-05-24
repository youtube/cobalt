// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/starboard/starboard_surface_texture_gl_owner.h"

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

StarboardSurfaceTextureGLOwner::StarboardSurfaceTextureGLOwner(
    std::unique_ptr<AbstractTextureAndroid> texture,
    scoped_refptr<SharedContextState> context_state)
    : TextureOwner(true /*binds_texture_on_update */,
                   std::move(texture),
                   std::move(context_state)),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()) {
  DCHECK(context_);
  DCHECK(surface_);
}

StarboardSurfaceTextureGLOwner::~StarboardSurfaceTextureGLOwner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Do ReleaseResources() if it hasn't already. This will do nothing if it
  // has already been destroyed.
  ReleaseResources();
}

void StarboardSurfaceTextureGLOwner::ReleaseResources() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void StarboardSurfaceTextureGLOwner::SetFrameAvailableCallback(
    const base::RepeatingClosure& frame_available_cb) {}

void StarboardSurfaceTextureGLOwner::RunWhenBufferIsAvailable(
    base::OnceClosure callback) {
  // SurfaceTexture can always render to front buffer.
  std::move(callback).Run();
}

gl::ScopedJavaSurface StarboardSurfaceTextureGLOwner::CreateJavaSurface()
    const {
  return gl::ScopedJavaSurface();
}

void StarboardSurfaceTextureGLOwner::UpdateTexImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void StarboardSurfaceTextureGLOwner::EnsureTexImageBound(GLuint service_id) {
  // We can't bind SurfaceTexture to different ids.
  DCHECK_EQ(service_id, GetTextureId());
}

void StarboardSurfaceTextureGLOwner::ReleaseBackBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

gl::GLContext* StarboardSurfaceTextureGLOwner::GetContext() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return context_.get();
}

gl::GLSurface* StarboardSurfaceTextureGLOwner::GetSurface() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return surface_.get();
}

std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
StarboardSurfaceTextureGLOwner::GetAHardwareBuffer() {
  NOTREACHED()
      << "Don't use AHardwareBuffers with StarboardSurfaceTextureGLOwner";
  return nullptr;
}

bool StarboardSurfaceTextureGLOwner::GetCodedSizeAndVisibleRect(
    gfx::Size rotated_visible_size,
    gfx::Size* coded_size,
    gfx::Rect* visible_rect) {
  DCHECK(coded_size);
  DCHECK(visible_rect);
  return true;
}

}  // namespace gpu
