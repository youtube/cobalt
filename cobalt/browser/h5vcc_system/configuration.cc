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

#include "cobalt/browser/h5vcc_system/configuration.h"

#include <string>

#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "build/build_config.h"
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
  DCHECK_EQ(std::string(configuration_api_->name),
            kCobaltExtensionConfigurationName)
      << "Unexpected extension name.";
  DCHECK_GE(configuration_api_->version, 1u) << "Unexpected extension version.";
}

Configuration::UserOnExitStrategy Configuration::CobaltUserOnExitStrategy() {
// TODO(b/385357645): fix calling the configuratino extension function.
#if BUILDFLAG(IS_ANDROID)
  return Configuration::UserOnExitStrategy::kMinimize;
#else
  return Configuration::UserOnExitStrategy::kClose;
#endif
}

}  // namespace configuration
}  // namespace cobalt
