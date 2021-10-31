/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "glimp/stub/egl/display_impl.h"

#include "glimp/stub/egl/pbuffer_surface_impl.h"
#include "glimp/stub/egl/window_surface_impl.h"
#include "glimp/stub/gles/context_impl.h"
#include "nb/scoped_ptr.h"
#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/types.h"

namespace glimp {
namespace egl {

DisplayImplStub::DisplayImplStub() {
  InitializeSupportedConfigs();
}

DisplayImplStub::~DisplayImplStub() {
  for (ConfigSet::iterator iter = supported_configs_.begin();
       iter != supported_configs_.end(); ++iter) {
    delete *iter;
  }
}

bool DisplayImpl::IsValidNativeDisplayType(
    EGLNativeDisplayType native_display) {
  return native_display == EGL_DEFAULT_DISPLAY;
}

nb::scoped_ptr<DisplayImpl> DisplayImpl::Create(
    EGLNativeDisplayType native_display) {
  SB_CHECK(IsValidNativeDisplayType(native_display));
  return nb::scoped_ptr<DisplayImpl>(new DisplayImplStub());
}

DisplayImpl::VersionInfo DisplayImplStub::GetVersionInfo() {
  DisplayImpl::VersionInfo version_info;
  version_info.major = 1;
  version_info.minor = 5;
  return version_info;
}

void DisplayImplStub::InitializeSupportedConfigs() {
  Config* config = new Config();
  (*config)[EGL_RED_SIZE] = 8;
  (*config)[EGL_GREEN_SIZE] = 8;
  (*config)[EGL_BLUE_SIZE] = 8;
  (*config)[EGL_ALPHA_SIZE] = 8;
  (*config)[EGL_BUFFER_SIZE] = 32;
  (*config)[EGL_LUMINANCE_SIZE] = 0;
  (*config)[EGL_STENCIL_SIZE] = 0;
  (*config)[EGL_COLOR_BUFFER_TYPE] = EGL_RGB_BUFFER;
  (*config)[EGL_CONFORMANT] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
  (*config)[EGL_RENDERABLE_TYPE] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
  (*config)[EGL_SURFACE_TYPE] = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
  (*config)[EGL_BIND_TO_TEXTURE_RGBA] = EGL_TRUE;

  supported_configs_.insert(config);
}

nb::scoped_ptr<gles::ContextImpl> DisplayImplStub::CreateContext(
    const Config* config,
    int gles_version) {
  if (gles_version == 2 || gles_version == 3) {
    return nb::scoped_ptr<gles::ContextImpl>(new gles::ContextImplStub());
  } else {
    return nb::scoped_ptr<gles::ContextImpl>();
  }
}

}  // namespace egl
}  // namespace glimp
