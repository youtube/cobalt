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
#include "starboard/log.h"

namespace glimp {
namespace egl {

Display::Display(base::scoped_ptr<DisplayImpl> display_impl)
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
  ValidatedConfigAttribs attribs;
  if (!ValidateConfigAttribList(attrib_list, &attribs)) {
    SetError(EGL_BAD_ATTRIBUTE);
    return false;
  }

  std::vector<Config*> configs_vector =
      FilterConfigs(impl_->GetSupportedConfigs(), attribs);
  SortConfigs(attribs, &configs_vector);

  *num_config = std::min(config_size, static_cast<int>(configs_vector.size()));

  if (configs) {
    for (int i = 0; i < *num_config; ++i) {
      configs[i] = ToEGLConfig(configs_vector[i]);
    }
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
  ValidatedSurfaceAttribs validated_attribs;
  if (!ValidateSurfaceAttribList(attrib_list, &validated_attribs)) {
    SetError(EGL_BAD_ATTRIBUTE);
    return EGL_NO_SURFACE;
  }

  if (!ConfigIsValid(config)) {
    SetError(EGL_BAD_CONFIG);
    return EGL_NO_SURFACE;
  }

  base::scoped_ptr<SurfaceImpl> surface_impl = impl_->CreateWindowSurface(
      reinterpret_cast<Config*>(config), win, validated_attribs);
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

}  // namespace egl
}  // namespace glimp
