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

namespace glimp {
namespace egl {

Display::Display(base::scoped_ptr<DisplayImpl> display_impl)
    : impl_(display_impl.Pass()) {}

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

bool Display::IsValidConfig(EGLConfig config) {
  const DisplayImpl::ConfigSet& supported_configs =
      impl_->GetSupportedConfigs();

  return supported_configs.find(reinterpret_cast<Config*>(config)) !=
         supported_configs.end();
}

}  // namespace egl
}  // namespace glimp
