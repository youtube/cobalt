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

#include "cobalt/h5vcc/h5vcc_settings.h"

#include <string.h>

namespace cobalt {
namespace h5vcc {

H5vccSettings::H5vccSettings(const SetSettingFunc& set_web_setting_func,
                             cobalt::media::MediaModule* media_module,
                             cobalt::network::NetworkModule* network_module,
#if SB_IS(EVERGREEN)
                             cobalt::updater::UpdaterModule* updater_module,
#endif
                             web::NavigatorUAData* user_agent_data,
                             script::GlobalEnvironment* global_environment)
    : set_web_setting_func_(set_web_setting_func),
      media_module_(media_module),
      network_module_(network_module),
#if SB_IS(EVERGREEN)
      updater_module_(updater_module),
#endif
      user_agent_data_(user_agent_data),
      global_environment_(global_environment) {
}

bool H5vccSettings::Set(const std::string& name, int32 value) const {
  const char kMediaPrefix[] = "Media.";
  const char kNavigatorUAData[] = "NavigatorUAData";
  const char kQUIC[] = "QUIC";

#if SB_IS(EVERGREEN)
  const char kUpdaterMinFreeSpaceBytes[] = "Updater.MinFreeSpaceBytes";
#endif

  if (set_web_setting_func_ && set_web_setting_func_.Run(name, value)) {
    return true;
  }

  if (name.rfind(kMediaPrefix, 0) == 0) {
    return media_module_ ? media_module_->SetConfiguration(
                               name.substr(strlen(kMediaPrefix)), value)
                         : false;
  }

  if (name.compare(kNavigatorUAData) == 0 && value == 1) {
    global_environment_->BindTo("userAgentData", user_agent_data_, "navigator");
    return true;
  }

  if (name.compare(kQUIC) == 0) {
    if (!network_module_) {
      return false;
    } else {
      network_module_->SetEnableQuic(value != 0);
      return true;
    }
  }

#if SB_IS(EVERGREEN)
  if (name.compare(kUpdaterMinFreeSpaceBytes) == 0) {
    updater_module_->SetMinFreeSpaceBytes(value);
    return true;
  }
#endif
  return false;
}

}  // namespace h5vcc
}  // namespace cobalt
