// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/configuration/configuration.h"

#include "base/memory/singleton.h"
#include "starboard/system.h"

namespace cobalt {
namespace configuration {

// static
Configuration* Configuration::GetInstance() {
  return base::Singleton<Configuration,
                         base::LeakySingletonTraits<Configuration>>::get();
}

Configuration::Configuration() {
  configuration_api_ = static_cast<const CobaltExtensionConfigurationApi*>(
      SbSystemGetExtension(kCobaltExtensionConfigurationName));
  if (!configuration_api_) {
    return;
  }
  CHECK_EQ(std::string(configuration_api_->name),
           kCobaltExtensionConfigurationName)
      << "Unexpected extension name.";
  CHECK_GE(configuration_api_->version, 1) << "Unexpected extension version.";
}

}  // namespace configuration
}  // namespace cobalt
