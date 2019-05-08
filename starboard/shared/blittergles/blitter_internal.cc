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

#include "starboard/shared/blittergles/blitter_internal.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "starboard/common/optional.h"
#include "starboard/log.h"
#include "starboard/once.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {
// Keep track of a global device registry that will be accessed by calls to
// create/destroy devices.
SbBlitterDeviceRegistry* s_device_registry = NULL;
SbOnceControl s_device_registry_once_control = SB_ONCE_INITIALIZER;

// Represents the rendering target.
struct BufferTarget {
  EGLSurface surface;

  GLint framebuffer_handle;
};

void EnsureDeviceRegistryInitialized() {
  s_device_registry = new SbBlitterDeviceRegistry();
  s_device_registry->default_device = NULL;
}

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

bool EnsureEGLConfigInitialized(SbBlitterDevicePrivate* device) {
  if (device->config) {
    return true;
  }

  starboard::optional<EGLConfig> config = ComputeEGLConfig(device->display);
  device->config = config ? *config : NULL;
  return config.has_engaged();
}

// If there's no config set on device at the time of SbBlitterContext
// creation, we defer intialization of the egl_context field. Since we need
// to bind at this point, we cannot defer anymore and must ensure it's
// initialized here.
bool EnsureEGLContextInitialized(SbBlitterContext context) {
  if (!EnsureEGLConfigInitialized(context->device)) {
    SB_DLOG(ERROR) << ": Failed to create config.";
    return false;
  }

  if (context->egl_context != EGL_NO_CONTEXT) {
    return true;
  }

  context->egl_context = eglCreateContext(
      context->device->display, context->device->config, EGL_NO_CONTEXT,
      starboard::shared::blittergles::kContextAttributeList);
  return eglGetError() == EGL_SUCCESS;
}

// This function assumes that config has been set on device.
bool EnsureDummySurfaceInitialized(SbBlitterContext context) {
  // TODO: Once SbBlitterCreateRenderTargetSurface() is implemented, see if
  // it's possible to bind a framebuffer with dummy_surface as EGL_NO_SURFACE.
  if (context->dummy_surface != EGL_NO_SURFACE) {
    return true;
  }

  EGLint surface_attrib_list[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  context->dummy_surface = eglCreatePbufferSurface(
      context->device->display, context->device->config, surface_attrib_list);
  return eglGetError() == EGL_SUCCESS;
}

// This function ensures that the render target has a valid EGLSurface and GL
// framebuffer.
starboard::optional<BufferTarget> GetEGLSurfaceAndGLFramebufferFromRenderTarget(
    SbBlitterContext context) {
  BufferTarget current_buffer_target;
  if (context->current_render_target->swap_chain != NULL) {
    current_buffer_target.surface =
        context->current_render_target->swap_chain->surface;
    current_buffer_target.framebuffer_handle = 0;
  } else {
    if (context->current_render_target->framebuffer_handle == 0) {
      SB_DLOG(ERROR) << ": Render targets from SbBlitterSurface must have a "
                     << "non-default GL framebuffer set.";
      return starboard::nullopt;
    }

    if (!EnsureDummySurfaceInitialized(context)) {
      SB_DLOG(ERROR) << ": Failed to create a dummy surface.";
      return starboard::nullopt;
    }
    current_buffer_target.surface = context->dummy_surface;
    current_buffer_target.framebuffer_handle =
        context->current_render_target->framebuffer_handle;
  }

  return current_buffer_target;
}

}  // namespace

const EGLint kContextAttributeList[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                        EGL_NONE};

const EGLint kConfigAttributeList[] = {EGL_RED_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_ALPHA_SIZE,
                                       8,
                                       EGL_BUFFER_SIZE,
                                       32,
                                       EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                       EGL_COLOR_BUFFER_TYPE,
                                       EGL_RGB_BUFFER,
                                       EGL_CONFORMANT,
                                       EGL_OPENGL_ES2_BIT,
                                       EGL_RENDERABLE_TYPE,
                                       EGL_OPENGL_ES2_BIT,
                                       EGL_NONE};

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry() {
  SbOnce(&s_device_registry_once_control, &EnsureDeviceRegistryInitialized);

  return s_device_registry;
}

bool MakeCurrent(SbBlitterContext context) {
  if (!EnsureEGLContextInitialized(context)) {
    SB_DLOG(ERROR) << ": Failed to create egl_context.";
    return false;
  }
  starboard::optional<BufferTarget> current_buffer_target =
      GetEGLSurfaceAndGLFramebufferFromRenderTarget(context);
  if (!current_buffer_target) {
    return false;
  }

  eglMakeCurrent(context->device->display, current_buffer_target->surface,
                 current_buffer_target->surface, context->egl_context);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to make the egl surface current.";
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, current_buffer_target->framebuffer_handle);
  if (glGetError() != GL_NO_ERROR) {
    SB_DLOG(ERROR) << ": Failed to bind gl framebuffer.";
    return false;
  }
  context->is_current = true;
  GL_CALL(glViewport(0, 0, context->current_render_target->width,
                     context->current_render_target->height));

  return true;
}

ScopedCurrentContext::ScopedCurrentContext(SbBlitterContext context)
    : context_(context), error_(false), was_current_(context_->is_current) {
  if (!context_->is_current) {
    starboard::ScopedLock lock(context_->device->mutex);
    error_ = !MakeCurrent(context_);
  }
}

ScopedCurrentContext::~ScopedCurrentContext() {
  if (!error_ && !was_current_) {
    starboard::ScopedLock lock(context_->device->mutex);
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    EGL_CALL(eglMakeCurrent(context_->device->display, EGL_NO_SURFACE,
                            EGL_NO_SURFACE, EGL_NO_CONTEXT));
    context_->is_current = false;
  }
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard
