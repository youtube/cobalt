// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_settings.h"

#include <string.h>

namespace cobalt {
namespace h5vcc {

H5vccSettings::H5vccSettings(media::MediaModule* media_module)
    : media_module_(media_module) {}

bool H5vccSettings::Set(const std::string& name, int32 value) const {
  const char kMediaPrefix[] = "Media.";

  if (strncmp(name.c_str(), kMediaPrefix, sizeof(kMediaPrefix) - 1) == 0) {
    return media_module_ ? media_module_->SetConfiguration(name, value) : false;
  }
  return false;
}

}  // namespace h5vcc
}  // namespace cobalt
