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
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "cobalt/browser/cpu_usage_tracker.h"
#include "cobalt/network/network_module.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {
namespace h5vcc {

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

bool H5vccSettings::Set(const std::string& name, SetValueType value) const {
  const char kMediaPrefix[] = "Media.";
  const char kMediaCodecBlockList[] = "MediaCodecBlockList";
  const char kNavigatorUAData[] = "NavigatorUAData";
  const char kQUIC[] = "QUIC";
  const char kHTTP3[] = "HTTP3";

#if SB_IS(EVERGREEN)
  const char kUpdaterMinFreeSpaceBytes[] = "Updater.MinFreeSpaceBytes";
#endif

  if (name == kMediaCodecBlockList && value.IsType<std::string>() &&
      value.AsType<std::string>().size() < 256) {
    can_play_type_handler_->SetDisabledMediaCodecs(value.AsType<std::string>());
    return true;
  }

  if (set_web_setting_func_ && value.IsType<int32>() &&
      set_web_setting_func_.Run(name, value.AsType<int32>())) {
    return true;
  }

  if (name.rfind(kMediaPrefix, 0) == 0 && value.IsType<int32>()) {
    return media_module_
               ? media_module_->SetConfiguration(
                     name.substr(strlen(kMediaPrefix)), value.AsType<int32>())
               : false;
  }

  if (name.compare(kNavigatorUAData) == 0 && value.IsType<int32>() &&
      value.AsType<int32>() == 1) {
    global_environment_->BindTo("userAgentData", user_agent_data_, "navigator");
    return true;
  }

  if (name.compare(kQUIC) == 0 && value.IsType<int32>()) {
    if (!persistent_settings_ || !network_module_) {
      return false;
    } else {
      persistent_settings_->Set(network::kQuicEnabledPersistentSettingsKey,
                                base::Value(value.AsType<int32>() != 0));
      // Tell NetworkModule (if exists) to re-query persistent settings.
      network_module_->SetEnableQuicFromPersistentSettings();
      return true;
    }
  }

  if (name.compare(kHTTP3) == 0 && value.IsType<int32>()) {
    if (!persistent_settings_ || !network_module_) {
      return false;
    } else {
      persistent_settings_->Set(network::kHttp3EnabledPersistentSettingsKey,
                                base::Value(value.AsType<int32>() != 0));
      network_module_->SetEnableHttp3FromPersistentSettings();
      return true;
    }
  }

  if (name.compare("cpu_usage_tracker_intervals") == 0 &&
      value.IsType<std::string>() && value.AsType<std::string>().size() < 512) {
    absl::optional<base::Value> config =
        base::JSONReader::Read(value.AsType<std::string>());
    browser::CpuUsageTracker::GetInstance()->UpdateConfig(
        config.has_value() ? std::move(*config) : base::Value());
    return true;
  }

#if SB_IS(EVERGREEN)
  if (name.compare(kUpdaterMinFreeSpaceBytes) == 0 && value.IsType<int32>()) {
    updater_module_->SetMinFreeSpaceBytes(value.AsType<int32>());
    return true;
  }
#endif
  return false;
}

void H5vccSettings::SetPersistentSettingAsInt(const std::string& key,
                                              int value) const {
  if (persistent_settings_) {
    persistent_settings_->Set(key, base::Value(value));
  }
}

int H5vccSettings::GetPersistentSettingAsInt(const std::string& key,
                                             int default_setting) const {
  if (persistent_settings_) {
    base::Value value;
    persistent_settings_->Get(key, &value);
    return value.GetIfInt().value_or(default_setting);
  }
  return default_setting;
}

}  // namespace h5vcc
}  // namespace cobalt
