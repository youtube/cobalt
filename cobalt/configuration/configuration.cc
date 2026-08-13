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

#include <string>
#include <string_view>

#include "base/memory/singleton.h"
#include "base/notreached.h"
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
  constexpr char kStop[] = "stop";
  constexpr char kSuspend[] = "suspend";
  constexpr char kNoExit[] = "noexit";

  if (!configuration_api_ || !configuration_api_->CobaltUserOnExitStrategy) {
    return Configuration::UserOnExitStrategy::kClose;
  }
  const char* strategy_str = configuration_api_->CobaltUserOnExitStrategy();
  if (!strategy_str) {
    return Configuration::UserOnExitStrategy::kClose;
  }
  std::string_view strategy = strategy_str;
  if (strategy == kStop) {
    return Configuration::UserOnExitStrategy::kClose;
  } else if (strategy == kSuspend) {
    return Configuration::UserOnExitStrategy::kMinimize;
  } else if (strategy == kNoExit) {
    return Configuration::UserOnExitStrategy::kNoExit;
  }
  LOG(ERROR) << "Invalid CobaltUserOnExitStrategy: " << strategy;
  return Configuration::UserOnExitStrategy::kClose;
}

int Configuration::CobaltLocalTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaCacheSizeInBytes();
  }
  return 1024 * 1024 * 16;
}

}  // namespace configuration
}  // namespace cobalt
