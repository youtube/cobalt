/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "glimp/egl/display.h"

#include <algorithm>
#include <vector>

#include "glimp/egl/config.h"
#include "glimp/egl/error.h"
#include "starboard/common/log.h"

namespace glimp {
namespace egl {

Display::Display(nb::scoped_ptr<DisplayImpl> display_impl)
    : impl_(display_impl.Pass()) {}

Display::~Display() {
  SB_DCHECK(active_surfaces_.empty());
}

void Display::GetVersionInfo(EGLint* major, EGLint* minor) {
  DisplayImpl::VersionInfo version_info = impl_->GetVersionInfo();
  if (major) {
    *major = version_info.major;
  }
  if (minor) {
    *minor = version_info.minor;
  }
}

bool Display::ChooseConfig(const EGLint* attrib_list,
                           EGLConfig* configs,
                           EGLint config_size,
                           EGLint* num_config) {
  if (!num_config) {
    SetError(EGL_BAD_PARAMETER);
    return false;
  }
  AttribMap attribs = ParseRawAttribList(attrib_list);
  if (!ValidateConfigAttribList(attribs)) {
    SetError(EGL_BAD_ATTRIBUTE);
    return false;
  }

  std::vector<Config*> configs_vector =
      FilterConfigs(impl_->GetSupportedConfigs(), attribs);
  SortConfigs(attribs, &configs_vector);

  if (configs) {
    *num_config =
        std::min(config_size, static_cast<int>(configs_vector.size()));
    for (int i = 0; i < *num_config; ++i) {
      configs[i] = ToEGLConfig(configs_vector[i]);
    }
  } else {
    *num_config = static_cast<int>(configs_vector.size());
  }

  return true;
}

bool Display::ConfigIsValid(EGLConfig config) {
  const DisplayImpl::ConfigSet& supported_configs =
      impl_->GetSupportedConfigs();

  return supported_configs.find(reinterpret_cast<Config*>(config)) !=
         supported_configs.end();
}

EGLSurface Display::CreateWindowSurface(EGLConfig config,
                                        EGLNativeWindowType win,
                                        const EGLint* attrib_list) {
  AttribMap attribs = ParseRawAttribList(attrib_list);
  if (!ValidateSurfaceAttribList(attribs)) {
    SetError(EGL_BAD_ATTRIBUTE);
    return EGL_NO_SURFACE;
  }

  if (!ConfigIsValid(config)) {
    SetError(EGL_BAD_CONFIG);
    return EGL_NO_SURFACE;
  }

  if (!((*reinterpret_cast<Config*>(config))[EGL_SURFACE_TYPE] |
        EGL_WINDOW_BIT)) {
    // The config used must have the EGL_WINDOW_BIT set in order for us to
    // be able to create windows.
    SetError(EGL_BAD_MATCH);
    return EGL_NO_SURFACE;
  }

  nb::scoped_ptr<SurfaceImpl> surface_impl = impl_->CreateWindowSurface(
      reinterpret_cast<Config*>(config), win, attribs);
  if (!surface_impl) {
    return EGL_NO_SURFACE;
  }

  Surface* surface = new Surface(surface_impl.Pass());
  active_surfaces_.insert(surface);

  return ToEGLSurface(surface);
}

EGLSurface Display::CreatePbufferSurface(EGLConfig config,
                                         const EGLint* attrib_list) {
  AttribMap attribs = ParseRawAttribList(attrib_list);
  if (!ValidateSurfaceAttribList(attribs)) {
    SetError(EGL_BAD_ATTRIBUTE);
    return EGL_NO_SURFACE;
  }

  if (!ConfigIsValid(config)) {
    SetError(EGL_BAD_CONFIG);
    return EGL_NO_SURFACE;
  }

  if (!((*reinterpret_cast<Config*>(config))[EGL_SURFACE_TYPE] |
        EGL_PBUFFER_BIT)) {
    // The config used must have the EGL_PBUFFER_BIT set in order for us to
    // be able to create pbuffers.
    SetError(EGL_BAD_MATCH);
    return EGL_NO_SURFACE;
  }

  nb::scoped_ptr<SurfaceImpl> surface_impl =
      impl_->CreatePbufferSurface(reinterpret_cast<Config*>(config), attribs);
  if (!surface_impl) {
    return EGL_NO_SURFACE;
  }

  Surface* surface = new Surface(surface_impl.Pass());
  active_surfaces_.insert(surface);

  return ToEGLSurface(surface);
}

bool Display::SurfaceIsValid(EGLSurface surface) {
  return active_surfaces_.find(FromEGLSurface(surface)) !=
         active_surfaces_.end();
}

bool Display::DestroySurface(EGLSurface surface) {
  if (!SurfaceIsValid(surface)) {
    SetError(EGL_BAD_SURFACE);
    return false;
  }

  Surface* surf = FromEGLSurface(surface);
  active_surfaces_.erase(surf);
  delete surf;
  return true;
}

namespace {
// Returns -1 if the context attributes are invalid.
int GetContextVersion(const EGLint* attrib_list) {
  AttribMap attribs = ParseRawAttribList(attrib_list);

  // According to
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglCreateContext.xhtml,
  // the default version of the GL ES context is 1.
  if (attribs.empty()) {
    return 1;
  }

  // EGL_CONTEXT_CLIENT_VERSION is the only valid attribute for CreateContext.
  AttribMap::const_iterator found = attribs.find(EGL_CONTEXT_CLIENT_VERSION);
  if (found == attribs.end()) {
    // If we didn't find it, and the attribute list is not empty (checked above)
    // then this is an invalid attribute list.
    return -1;
  } else {
    return found->second;
  }
}
}  // namespace

EGLContext Display::CreateContext(EGLConfig config,
                                  EGLContext share_context,
                                  const EGLint* attrib_list) {
  // glimp only supports GL ES versions 2 and 3.
  int context_version = GetContextVersion(attrib_list);
  if (context_version != 2 && context_version != 3) {
    SetError(EGL_BAD_ATTRIBUTE);
    return EGL_NO_CONTEXT;
  }

  if (!ConfigIsValid(config)) {
    SetError(EGL_BAD_CONFIG);
    return EGL_NO_CONTEXT;
  }

  // Ensure that |share_context| is either unspecified, or valid.
  gles::Context* share = NULL;
  if (share_context != EGL_NO_CONTEXT) {
    if (!ContextIsValid(share_context)) {
      SetError(EGL_BAD_CONTEXT);
      return EGL_NO_CONTEXT;
    }
    share = reinterpret_cast<gles::Context*>(share_context);
  }

  nb::scoped_ptr<gles::ContextImpl> context_impl =
      impl_->CreateContext(reinterpret_cast<Config*>(config), context_version);
  if (!context_impl) {
    return EGL_NO_CONTEXT;
  }

  gles::Context* context = new gles::Context(context_impl.Pass(), share);
  active_contexts_.insert(context);

  return reinterpret_cast<EGLContext>(context);
}

bool Display::ContextIsValid(EGLContext context) {
  return active_contexts_.find(reinterpret_cast<gles::Context*>(context)) !=
         active_contexts_.end();
}

bool Display::DestroyContext(EGLContext ctx) {
  if (!ContextIsValid(ctx)) {
    SetError(EGL_BAD_CONTEXT);
    return false;
  }

  gles::Context* context = reinterpret_cast<gles::Context*>(ctx);
  active_contexts_.erase(context);
  delete context;
  return true;
}

bool Display::MakeCurrent(EGLSurface draw, EGLSurface read, EGLContext ctx) {
  if (draw == EGL_NO_SURFACE && read == EGL_NO_SURFACE &&
      ctx == EGL_NO_CONTEXT) {
    if (!ContextIsValid(reinterpret_cast<EGLContext>(
            gles::Context::GetTLSCurrentContext()))) {
      SB_DLOG(WARNING)
          << "Attempted to release a context not owned by this display.";
      SetError(EGL_BAD_CONTEXT);
      return false;
    }
    gles::Context::ReleaseTLSCurrentContext();
    return true;
  }

  if (!ContextIsValid(ctx)) {
    SetError(EGL_BAD_CONTEXT);
    return false;
  }

  if (!SurfaceIsValid(draw)) {
    SetError(EGL_BAD_SURFACE);
    return false;
  }

  if (!SurfaceIsValid(read)) {
    SetError(EGL_BAD_SURFACE);
    return false;
  }

  return gles::Context::SetTLSCurrentContext(
      reinterpret_cast<gles::Context*>(ctx), FromEGLSurface(draw),
      FromEGLSurface(read));
}

bool Display::SwapBuffers(EGLSurface surface) {
  if (!SurfaceIsValid(surface)) {
    SetError(EGL_BAD_SURFACE);
    return false;
  }
  Surface* surface_object = FromEGLSurface(surface);

  gles::Context* current_context = gles::Context::GetTLSCurrentContext();
  if (!ContextIsValid(reinterpret_cast<EGLContext>(current_context))) {
    // The specification for eglSwapBuffers() does not explicitly state that
    // the surface's context needs to be current when eglSwapBuffers() is
    // called, but we enforce this in glimp as it is a very typical use-case and
    // it considerably simplifies the process.
    SB_DLOG(WARNING)
        << "eglSwapBuffers() called when no or an invalid context was current.";
    SetError(EGL_BAD_SURFACE);
    return false;
  }

  if (current_context->draw_surface() != surface_object) {
    SB_DLOG(WARNING)
        << "eglSwapBuffers() called on a surface that is not the draw surface "
        << "of the current context.";
    SetError(EGL_BAD_SURFACE);
    return false;
  }

  current_context->SwapBuffers();
  return true;
}

bool Display::SwapInterval(EGLint interval) {
  return impl_->SetSwapInterval(interval);
}

}  // namespace egl
}  // namespace glimp
