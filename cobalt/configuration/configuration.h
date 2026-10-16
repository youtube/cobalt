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

#ifndef COBALT_CONFIGURATION_CONFIGURATION_H_
#define COBALT_CONFIGURATION_CONFIGURATION_H_

#include "base/memory/raw_ptr.h"
#include "starboard/extension/configuration.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace configuration {

// The Configuration changes certain Cobalt features as specified by the
// platform. This class picks up values set in the
// CobaltConfigurationExtensionApi if it is implemented by the platform and
// will otherwise use default configurations.
class Configuration {
 public:
  enum class UserOnExitStrategy {
    kClose,
    kMinimize,
    kNoExit,
  };

  static Configuration* GetInstance();

  UserOnExitStrategy CobaltUserOnExitStrategy();
  int CobaltLocalTypefaceCacheSizeInBytes();

 private:
  Configuration();
  Configuration(const Configuration&) = delete;
  Configuration& operator=(const Configuration&) = delete;

  friend struct base::DefaultSingletonTraits<Configuration>;
  raw_ptr<const CobaltExtensionConfigurationApi> configuration_api_;
};

}  // namespace configuration
}  // namespace cobalt

#endif  // COBALT_CONFIGURATION_CONFIGURATION_H_
