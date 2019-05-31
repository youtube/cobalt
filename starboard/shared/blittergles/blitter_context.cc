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

#include <memory>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/optional.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/color_shader_program.h"
#include "starboard/shared/gles/gl_call.h"

namespace {

// Computes the best-guess config.
starboard::optional<EGLConfig> ComputeEGLConfig(EGLDisplay display) {
  EGLint num_configs;
  EGLConfig config;
  EGL_CALL(eglChooseConfig(display,
                           starboard::shared::blittergles::kConfigAttributeList,
                           &config, 1, &num_configs));
  if (num_configs != 1) {
    SB_DLOG(ERROR) << ": Couldn't find a config compatible with the specified "
                   << "attributes.";
    return starboard::nullopt;
  }

  return config;
}

bool EnsureEGLConfigInitialized(SbBlitterContext context) {
  if (context->device->config) {
    return true;
  }

  starboard::optional<EGLConfig> config =
      ComputeEGLConfig(context->device->display);
  context->device->config = config ? *config : NULL;
  return config.has_engaged();
}

void DeleteFramebufferFromRenderTarget(SbBlitterRenderTarget render_target) {
  if (render_target->framebuffer_handle != 0) {
    GL_CALL(glDeleteFramebuffers(1, &render_target->framebuffer_handle));
    render_target->framebuffer_handle = 0;
  }

  if (render_target->surface->color_texture_handle != 0) {
    GL_CALL(glDeleteTextures(1, &render_target->surface->color_texture_handle));
    render_target->surface->color_texture_handle = 0;
  }
}

// Mutates render_target to have a framebuffer set and mutates surface to have
// a texture set.
bool SetFramebufferOnRenderTarget(SbBlitterRenderTarget render_target) {
  glGenFramebuffers(1, &render_target->framebuffer_handle);
  if (glGetError() != GL_NO_ERROR) {
    SB_DLOG(ERROR) << ": Error creating new framebuffer.";
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, render_target->framebuffer_handle);
  if (glGetError() != GL_NO_ERROR) {
    DeleteFramebufferFromRenderTarget(render_target);
    SB_DLOG(ERROR) << ": Error binding framebuffer.";
    return false;
  }

  glGenTextures(1, &render_target->surface->color_texture_handle);
  if (glGetError() != GL_NO_ERROR) {
    DeleteFramebufferFromRenderTarget(render_target);
    SB_DLOG(ERROR) << ": Error creating new texture.";
    return false;
  }
  glBindTexture(GL_TEXTURE_2D, render_target->surface->color_texture_handle);
  if (glGetError() != GL_NO_ERROR) {
    DeleteFramebufferFromRenderTarget(render_target);
    SB_DLOG(ERROR) << ": Error binding new texture.";
    return false;
  }
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, render_target->width,
               render_target->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  if (glGetError() != GL_NO_ERROR) {
    DeleteFramebufferFromRenderTarget(render_target);
    SB_DLOG(ERROR) << ": Error allocating new texture backing for framebuffer.";
    return false;
  }

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         render_target->surface->color_texture_handle, 0);
  if (glGetError() != GL_NO_ERROR) {
    DeleteFramebufferFromRenderTarget(render_target);
    SB_DLOG(ERROR) << ": Error drawing empty image to framebuffer.";
    return false;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    DeleteFramebufferFromRenderTarget(render_target);
    SB_DLOG(ERROR) << ": Failed to create framebuffer.";
    return false;
  }

  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  return true;
}

starboard::optional<GLuint> GetFramebuffer(
    SbBlitterRenderTarget render_target) {
  GLuint framebuffer_handle;
  if (render_target->swap_chain != NULL) {
    return 0;
  } else if (render_target->framebuffer_handle != 0 ||
             SetFramebufferOnRenderTarget(render_target)) {
    return render_target->framebuffer_handle;
  } else {
    return starboard::nullopt;
  }
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
  // Defer creation of egl context if config hasn't been initialized. It's
  // possible that the egl context will eventually be created with a config that
  // doesn't check for eglCreateWindowSurface() compatibility.
  if (device->config == NULL) {
    egl_context_ = EGL_NO_CONTEXT;
  } else {
    starboard::ScopedLock lock(device->mutex);
    egl_context_ =
        eglCreateContext(device->display, device->config, EGL_NO_CONTEXT,
                         starboard::shared::blittergles::kContextAttributeList);
    error_ = egl_context_ == EGL_NO_CONTEXT;
  }

  SB_DCHECK(device->context == kSbBlitterInvalidContext);
  device->context = this;
}

SbBlitterContextPrivate::~SbBlitterContextPrivate() {
  if (color_shader_.get() != nullptr) {
    color_shader_.reset();
  }

  starboard::ScopedLock lock(device->mutex);
  if (egl_context_ != EGL_NO_CONTEXT) {
    // For now, we assume context is already unbound, as we bind and unbind
    // context after every Blitter API call that uses it.
    // TODO: Optimize eglMakeCurrent calls, so rebinding is not needed for every
    // API call.
    EGL_CALL(eglDestroyContext(device->display, egl_context_));
  }

  if (dummy_surface_ != EGL_NO_SURFACE) {
    EGL_CALL(eglDestroySurface(device->display, dummy_surface_));
  }
  SB_DCHECK(device->context == this);
  device->context = kSbBlitterInvalidContext;
}

const starboard::shared::blittergles::ColorShaderProgram&
SbBlitterContextPrivate::GetColorShaderProgram() {
  SB_DCHECK(is_current);
  if (color_shader_.get() == nullptr) {
    color_shader_.reset(
        new starboard::shared::blittergles::ColorShaderProgram());
  }
  return *color_shader_.get();
}

// If there's no config set on device at the time of SbBlitterContext
// creation, we defer intialization of the egl_context field. Since we need
// to bind at this point, we cannot defer anymore and must ensure it's
// initialized here.
bool SbBlitterContextPrivate::EnsureEGLContextInitialized() {
  if (!EnsureEGLConfigInitialized(this)) {
    SB_DLOG(ERROR) << ": Failed to create config.";
    return false;
  }

  if (egl_context_ != EGL_NO_CONTEXT) {
    return true;
  }

  egl_context_ =
      eglCreateContext(device->display, device->config, EGL_NO_CONTEXT,
                       starboard::shared::blittergles::kContextAttributeList);
  return eglGetError() == EGL_SUCCESS;
}

// Assumes that config has been set on device.
bool SbBlitterContextPrivate::EnsureDummySurfaceInitialized() {
  if (dummy_surface_ != EGL_NO_SURFACE) {
    return true;
  }

  EGLint surface_attrib_list[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  dummy_surface_ = eglCreatePbufferSurface(device->display, device->config,
                                           surface_attrib_list);
  return eglGetError() == EGL_SUCCESS;
}

starboard::optional<EGLSurface> SbBlitterContextPrivate::GetEGLSurface(
    SbBlitterRenderTarget render_target) {
  if (render_target->swap_chain != NULL) {
    return render_target->swap_chain->surface;
  }

  if (!EnsureDummySurfaceInitialized()) {
    SB_DLOG(ERROR) << ": Failed to create a dummy surface.";
    return starboard::nullopt;
  }

  return dummy_surface_;
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
  if (!EnsureEGLContextInitialized()) {
    SB_DLOG(ERROR) << ": Failed to create egl_context.";
    return false;
  }

  starboard::optional<EGLSurface> egl_surface = GetEGLSurface(render_target);
  if (!egl_surface) {
    return false;
  }
  eglMakeCurrent(device->display, *egl_surface, *egl_surface, egl_context_);
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
