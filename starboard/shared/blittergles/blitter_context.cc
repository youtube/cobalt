// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/blittergles/blitter_context.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <utility>

#include "starboard/common/log.h"
#include "starboard/shared/blittergles/blit_shader_program.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/color_shader_program.h"
#include "starboard/shared/gles/gl_call.h"

namespace {

// Sets the framebuffer handle on the given render target, and also attaches
// a color texture to that framebuffer.
bool SetFramebufferAndTexture(SbBlitterRenderTarget render_target) {
  glGenFramebuffers(1, &render_target->framebuffer_handle);
  if (render_target->framebuffer_handle == 0) {
    SB_DLOG(ERROR) << ": Error creating new framebuffer.";
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, render_target->framebuffer_handle);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    SB_DLOG(ERROR) << ": Error binding framebuffer.";
    return false;
  }

  if (!render_target->surface->EnsureInitialized()) {
    GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    return false;
  }
  GL_CALL(glBindTexture(GL_TEXTURE_2D,
                        render_target->surface->color_texture_handle));
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         render_target->surface->color_texture_handle, 0);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    GL_CALL(glDeleteTextures(1, &render_target->surface->color_texture_handle));
    SB_DLOG(ERROR) << ": Error drawing empty image to framebuffer.";
    return false;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    GL_CALL(glDeleteTextures(1, &render_target->surface->color_texture_handle));
    SB_DLOG(ERROR) << ": Failed to create framebuffer.";
    return false;
  }
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  return true;
}

starboard::optional<GLuint> GetFramebuffer(
    SbBlitterRenderTarget render_target) {
  if (render_target->swap_chain != NULL) {
    return 0;
  }

  if (render_target->framebuffer_handle == 0 &&
      !SetFramebufferAndTexture(render_target)) {
    return starboard::nullopt;
  }
  return render_target->framebuffer_handle;
}

}  // namespace

SbBlitterContextPrivate::SbBlitterContextPrivate(SbBlitterDevice device)
    : is_current(false),
      current_render_target(kSbBlitterInvalidRenderTarget),
      device(device),
      dummy_surface_(EGL_NO_SURFACE),
      blending_enabled(false),
      modulate_blits_with_color(false),
      current_color(SbBlitterColorFromRGBA(255, 255, 255, 255)),
      scissor(SbBlitterMakeRect(0, 0, 0, 0)),
      error_(false) {
  starboard::ScopedLock lock(device->mutex);
  egl_context_ =
      eglCreateContext(device->display, device->config, EGL_NO_CONTEXT,
                       starboard::shared::blittergles::kContextAttributeList);
  EGLint surface_attrib_list[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  dummy_surface_ = eglCreatePbufferSurface(device->display, device->config,
                                           surface_attrib_list);
  error_ = egl_context_ == EGL_NO_CONTEXT || dummy_surface_ == EGL_NO_SURFACE;
  EGL_CALL(eglMakeCurrent(device->display, dummy_surface_, dummy_surface_,
                          egl_context_));
  color_shader_.reset(new starboard::shared::blittergles::ColorShaderProgram());
  blit_shader_.reset(new starboard::shared::blittergles::BlitShaderProgram());
  EGL_CALL(eglMakeCurrent(device->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                          EGL_NO_CONTEXT));
  SB_DCHECK(device->context == kSbBlitterInvalidContext);
  device->context = this;
}

SbBlitterContextPrivate::~SbBlitterContextPrivate() {
  color_shader_.reset();
  blit_shader_.reset();

  starboard::ScopedLock lock(device->mutex);

  // For now, we assume context is already unbound, as we bind and unbind
  // context after every Blitter API call that uses it.
  // TODO: Optimize eglMakeCurrent calls, so rebinding is not needed for every
  // API call.
  EGL_CALL(eglDestroyContext(device->display, egl_context_));
  EGL_CALL(eglDestroySurface(device->display, dummy_surface_));

  SB_DCHECK(device->context == this);
  device->context = kSbBlitterInvalidContext;
}

const starboard::shared::blittergles::ColorShaderProgram&
    SbBlitterContextPrivate::GetColorShaderProgram() {
  SB_DCHECK(is_current);
  return *color_shader_.get();
}

const starboard::shared::blittergles::BlitShaderProgram&
    SbBlitterContextPrivate::GetBlitShaderProgram() {
  SB_DCHECK(is_current);
  return *blit_shader_.get();
}

bool SbBlitterContextPrivate::MakeCurrent() {
  if (!is_current) {
    return MakeCurrentWithRenderTarget(current_render_target);
  }

  return true;
}

bool SbBlitterContextPrivate::MakeCurrentWithRenderTarget(
    SbBlitterRenderTarget render_target) {
  starboard::ScopedLock lock(device->mutex);
  EGLSurface egl_surface = render_target->swap_chain != NULL
                               ? render_target->swap_chain->surface
                               : dummy_surface_;
  eglMakeCurrent(device->display, egl_surface, egl_surface, egl_context_);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to make the egl surface current.";
    return false;
  }

  starboard::optional<GLuint> framebuffer = GetFramebuffer(render_target);
  if (!framebuffer) {
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer);
  if (glGetError() != GL_NO_ERROR) {
    SB_DLOG(ERROR) << ": Failed to bind framebuffer.";
    return false;
  }
  GL_CALL(glViewport(0, 0, render_target->width, render_target->height));

  is_current = true;
  return true;
}

SbBlitterContextPrivate::ScopedCurrentContext::ScopedCurrentContext(
    SbBlitterContext context)
    : context_(context),
      error_(false),
      was_current_(context_->is_current),
      previous_render_target_(context_->current_render_target) {
  error_ = !context_->MakeCurrent();
}

SbBlitterContextPrivate::ScopedCurrentContext::ScopedCurrentContext(
    SbBlitterContext context,
    SbBlitterRenderTarget render_target)
    : context_(context),
      error_(false),
      was_current_(context_->is_current),
      previous_render_target_(context_->current_render_target) {
  error_ = !context_->MakeCurrentWithRenderTarget(render_target);
}

SbBlitterContextPrivate::ScopedCurrentContext::~ScopedCurrentContext() {
  if (was_current_) {
    context_->MakeCurrentWithRenderTarget(previous_render_target_);
  } else {
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    starboard::ScopedLock lock(context_->device->mutex);
    EGL_CALL(eglMakeCurrent(context_->device->display, EGL_NO_SURFACE,
                            EGL_NO_SURFACE, EGL_NO_CONTEXT));
    context_->is_current = false;
  }
}
