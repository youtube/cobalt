// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

// Dial requires some system information exposed to the world, for example
// the device name, etc. This class lets it be configurable, but throw default
// values from implementation.

#ifndef COBALT_NETWORK_DIAL_DIAL_SYSTEM_CONFIG_H_
#define COBALT_NETWORK_DIAL_DIAL_SYSTEM_CONFIG_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace network {

class DialSystemConfig {
 public:
  static DialSystemConfig* GetInstance();

  // Stores the pointers to various system info.
  const char* friendly_name() const { return friendly_name_.c_str(); }

  const char* manufacturer_name() const { return manufacturer_name_.c_str(); }

  const char* model_name() const { return model_name_.c_str(); }

  // Gets the model uuid.
  const char* model_uuid() const;

 private:
  DialSystemConfig();

  friend struct base::DefaultSingletonTraits<DialSystemConfig>;

  static void CreateDialUuid();

  // These 4 functions must be defined by a platform-specific source file.
  static std::string GetFriendlyName();
  static std::string GetManufacturerName();
  static std::string GetModelName();
  static std::string GeneratePlatformUuid();

  static const int kMaxNameSize = 64;

  std::string friendly_name_;
  std::string manufacturer_name_;
  std::string model_name_;

  // DISALLOW_COPY_AND_ASSIGN(DialSystemConfig);
  mutable base::Lock lock_;
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DIAL_DIAL_SYSTEM_CONFIG_H_
