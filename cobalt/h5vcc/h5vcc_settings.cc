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

#include <memory>

#include "cobalt/network/network_module.h"

namespace cobalt {
namespace h5vcc {

namespace {
// Only including needed video combinations for the moment.
// option 0 disables all video codecs except h264
// option 1 disables all video codecs except av1
// option 2 disables all video codecs except vp9
constexpr std::array<const char*, 3> kDisableCodecCombinations{
    {"av01;hev1;hvc1;vp09;vp8.vp9", "avc1;avc3;hev1;hvc1;vp09;vp8;vp9",
     "av01;avc1;avc3;hev1;hvc1;vp8"}};
};  // namespace

H5vccSettings::H5vccSettings(
    const SetSettingFunc& set_web_setting_func,
    cobalt::media::MediaModule* media_module,
    cobalt::media::CanPlayTypeHandler* can_play_type_handler,
    cobalt::network::NetworkModule* network_module,
#if SB_IS(EVERGREEN)
    cobalt::updater::UpdaterModule* updater_module,
#endif
    web::NavigatorUAData* user_agent_data,
    script::GlobalEnvironment* global_environment,
    persistent_storage::PersistentSettings* persistent_settings)
    : set_web_setting_func_(set_web_setting_func),
      media_module_(media_module),
      can_play_type_handler_(can_play_type_handler),
      network_module_(network_module),
#if SB_IS(EVERGREEN)
      updater_module_(updater_module),
#endif
      user_agent_data_(user_agent_data),
      global_environment_(global_environment),
      persistent_settings_(persistent_settings) {
}

bool H5vccSettings::Set(const std::string& name, int32 value) const {
  const char kMediaPrefix[] = "Media.";
  const char kDisableMediaCodec[] = "DisableMediaCodec";
  const char kNavigatorUAData[] = "NavigatorUAData";
  const char kClientHintHeaders[] = "ClientHintHeaders";
  const char kQUIC[] = "QUIC";

#if SB_IS(EVERGREEN)
  const char kUpdaterMinFreeSpaceBytes[] = "Updater.MinFreeSpaceBytes";
#endif

  if (name == kDisableMediaCodec &&
      value < static_cast<int32>(kDisableCodecCombinations.size())) {
    can_play_type_handler_->SetDisabledMediaCodecs(
        kDisableCodecCombinations[value]);
    return true;
  }

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

  if (name.compare(kClientHintHeaders) == 0) {
    if (!persistent_settings_) {
      return false;
    } else {
      persistent_settings_->SetPersistentSetting(
          network::kClientHintHeadersEnabledPersistentSettingsKey,
          std::make_unique<base::Value>(value));
      // Tell NetworkModule (if exists) to re-query persistent settings.
      if (network_module_) {
        network_module_
            ->SetEnableClientHintHeadersFlagsFromPersistentSettings();
      }
      return true;
    }
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
