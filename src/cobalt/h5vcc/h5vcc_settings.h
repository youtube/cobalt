// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_SETTINGS_H_
#define COBALT_H5VCC_H5VCC_SETTINGS_H_

#include <string>

#include "cobalt/dom/navigator_ua_data.h"
#include "cobalt/media/media_module.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

// This class can be used to modify system settings from JS.
// Setting enumeration and retrieving are explicitly excluded in the initial
// version to avoid being abused.
class H5vccSettings : public script::Wrappable {
 public:
  explicit H5vccSettings(media::MediaModule* media_module,
                         cobalt::network::NetworkModule* network_module,
                         dom::NavigatorUAData* user_agent_data,
                         script::GlobalEnvironment* global_environment);

  // Returns true when the setting is set successfully or if the setting has
  // already been set to the expected value.  Returns false when the setting is
  // invalid or not set to the expected value.
  bool Set(const std::string& name, int32 value) const;

  DEFINE_WRAPPABLE_TYPE(H5vccSettings);

 private:
  media::MediaModule* media_module_;
  cobalt::network::NetworkModule* network_module_ = nullptr;
  dom::NavigatorUAData* user_agent_data_;
  script::GlobalEnvironment* global_environment_;

  DISALLOW_COPY_AND_ASSIGN(H5vccSettings);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_SETTINGS_H_
