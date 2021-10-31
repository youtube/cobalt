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

#include "starboard/common/log.h"
#include "starboard/common/recursive_mutex.h"
#include "starboard/once.h"
#include "starboard/shared/blittergles/blit_shader_program.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/blitter_surface.h"
#include "starboard/shared/blittergles/color_shader_program.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

// Keep track of a global context registry that will be accessed by calls to
// create/destroy contexts.
SbBlitterContextRegistry* s_context_registry = NULL;
SbOnceControl s_context_registry_once_control = SB_ONCE_INITIALIZER;

void EnsureContextRegistryInitialized() {
  s_context_registry = new SbBlitterContextRegistry();
  s_context_registry->context = NULL;
  s_context_registry->in_use = false;
}

SbBlitterContextRegistry* GetBlitterContextRegistry() {
  SbOnce(&s_context_registry_once_control, &EnsureContextRegistryInitialized);

  return s_context_registry;
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

SbBlitterContextPrivate::SbBlitterContextPrivate(SbBlitterDevice device)
    : is_current(false),
      current_render_target(kSbBlitterInvalidRenderTarget),
      device(device),
      dummy_surface_(EGL_NO_SURFACE),
      blending_enabled(false),
      modulate_blits_with_color(false),
      current_rgba{1.0f, 1.0f, 1.0f, 1.0f},
      scissor(SbBlitterMakeRect(0, 0, 0, 0)),
      gl_blend_enabled_(false),
      error_(false) {
  starboard::ScopedRecursiveLock lock(device->mutex);
  egl_context_ =
      eglCreateContext(device->display, device->config, EGL_NO_CONTEXT,
                       starboard::shared::blittergles::kContextAttributeList);
  EGLint surface_attrib_list[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  dummy_surface_ = eglCreatePbufferSurface(device->display, device->config,
                                           surface_attrib_list);
  error_ = egl_context_ == EGL_NO_CONTEXT || dummy_surface_ == EGL_NO_SURFACE;
  eglMakeCurrent(device->display, dummy_surface_, dummy_surface_, egl_context_);
  error_ |= eglGetError() != EGL_SUCCESS;
  color_shader_.reset(new starboard::shared::blittergles::ColorShaderProgram());
  blit_shader_.reset(new starboard::shared::blittergles::BlitShaderProgram());
  GL_CALL(glEnable(GL_SCISSOR_TEST));
  GL_CALL(glDisable(GL_BLEND));
  EGL_CALL(eglMakeCurrent(device->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                          EGL_NO_CONTEXT));
}

SbBlitterContextPrivate::~SbBlitterContextPrivate() {
  GL_CALL(glDisable(GL_SCISSOR_TEST));
  color_shader_.reset();
  blit_shader_.reset();

  starboard::ScopedRecursiveLock lock(device->mutex);

  // For now, we assume context is already unbound, as we bind and unbind
  // context after every Blitter API call that uses it.
  // TODO: Optimize eglMakeCurrent calls, so rebinding is not needed for every
  // API call.
  EGL_CALL(eglDestroyContext(device->display, egl_context_));
  EGL_CALL(eglDestroySurface(device->display, dummy_surface_));
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

void SbBlitterContextPrivate::PrepareDrawState() {
  GL_CALL(glScissor(
      scissor.x,
      current_render_target->height - scissor.y - (scissor.height + 1),
      scissor.width + 1, scissor.height + 1));

  // Don't force unnecessary blend state changes.
  if (gl_blend_enabled_ == blending_enabled) {
    return;
  } else if (blending_enabled) {
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
  } else {
    GL_CALL(glDisable(GL_BLEND));
  }
  gl_blend_enabled_ = blending_enabled;
}

bool SbBlitterContextPrivate::MakeCurrent() {
  if (!is_current) {
    return MakeCurrentWithRenderTarget(current_render_target);
  }

  return true;
}

bool SbBlitterContextPrivate::MakeCurrentWithRenderTarget(
    SbBlitterRenderTarget render_target) {
  EGLSurface egl_surface = render_target->swap_chain == NULL
                               ? dummy_surface_
                               : render_target->swap_chain->surface;
  eglMakeCurrent(device->display, egl_surface, egl_surface, egl_context_);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to make the egl surface current.";
    return false;
  }

  GLuint framebuffer = 0;
  if (render_target->swap_chain == NULL) {
    if (render_target->framebuffer_handle == 0) {
      return false;
    }
    framebuffer = render_target->framebuffer_handle;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  if (glGetError() != GL_NO_ERROR) {
    SB_DLOG(ERROR) << ": Failed to bind framebuffer.";
    return false;
  }
  GL_CALL(glViewport(0, 0, render_target->width, render_target->height));

  is_current = true;
  return true;
}

SbBlitterContextPrivate::ScopedCurrentContext::ScopedCurrentContext()
    : error_(false) {
  starboard::shared::blittergles::SbBlitterContextRegistry* context_registry =
      starboard::shared::blittergles::GetBlitterContextRegistry();
  context_registry->context->device->mutex.Acquire();
  context_ = context_registry->context;
  was_current_ = context_->is_current;
  previous_render_target_ = context_->current_render_target;
  eglMakeCurrent(context_->device->display, context_->dummy_surface_,
                 context_->dummy_surface_, context_->egl_context_);
  error_ = eglGetError() != EGL_SUCCESS;
  context_->is_current = true;
}

SbBlitterContextPrivate::ScopedCurrentContext::ScopedCurrentContext(
    SbBlitterContext context)
    : error_(false) {
  context->device->mutex.Acquire();
  context_ = context;
  was_current_ = context_->is_current;
  previous_render_target_ = context_->current_render_target;
  error_ = !context_->MakeCurrent();
}

SbBlitterContextPrivate::ScopedCurrentContext::ScopedCurrentContext(
    SbBlitterContext context,
    SbBlitterRenderTarget render_target)
    : error_(false) {
  context->device->mutex.Acquire();
  context_ = context;
  was_current_ = context_->is_current;
  previous_render_target_ = context_->current_render_target;
  error_ = !context_->MakeCurrentWithRenderTarget(render_target);
}

SbBlitterContextPrivate::ScopedCurrentContext::~ScopedCurrentContext() {
  if (was_current_ &&
      previous_render_target_ != kSbBlitterInvalidRenderTarget) {
    context_->MakeCurrentWithRenderTarget(previous_render_target_);
  } else {
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    EGL_CALL(eglMakeCurrent(context_->device->display, EGL_NO_SURFACE,
                            EGL_NO_SURFACE, EGL_NO_CONTEXT));
  }
  context_->is_current = was_current_;
  context_->device->mutex.Release();
}
