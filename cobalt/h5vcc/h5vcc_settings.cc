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

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace h5vcc {

namespace {
constexpr std::array<const char*, 8> kCodecList{
    {"av01", "avc1", "avc3", "hev1", "hvc1", "vp09", "vp8", "vp9"}};
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

bool H5vccSettings::Set(const std::string& name, SetValueType value) const {
  const char kMediaPrefix[] = "Media.";
  const char kMediaCodecAllowList[] = "MediaCodecAllowList";
  const char kNavigatorUAData[] = "NavigatorUAData";
  const char kClientHintHeaders[] = "ClientHintHeaders";
  const char kQUIC[] = "QUIC";

#if SB_IS(EVERGREEN)
  const char kUpdaterMinFreeSpaceBytes[] = "Updater.MinFreeSpaceBytes";
#endif

  if (name == kMediaCodecAllowList && value.IsType<std::string>()) {
    std::vector<std::string> allowed_media_codecs =
        base::SplitString(value.AsType<std::string>(), ";",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    // Allow only the codecs in the list.
    // If it is emtpy string, it allows all codecs.
    // Examples: input string seperated by semicolon
    // "vp09;vp9": allow only vp9 codec
    // "": allow all codecs
    if (allowed_media_codecs.size() > 0) {
      std::unordered_set<std::string> allowed_media_codecs_set(
          allowed_media_codecs.begin(), allowed_media_codecs.end());
      std::vector<std::string> blocked_media_codecs;
      for (auto& codec : kCodecList) {
        if (allowed_media_codecs_set.count(codec) <= 0) {
          blocked_media_codecs.push_back(codec);
        }
      }
      std::string blocked_media_codec_combinations =
          base::JoinString(blocked_media_codecs, ";");
      LOG(INFO) << "Codec (" << blocked_media_codec_combinations
                << ") is blocked via console/command line.";
      can_play_type_handler_->SetDisabledMediaCodecs(
          blocked_media_codec_combinations);
    } else {
      LOG(INFO) << "Allowed All Codec via console/command line.";
      can_play_type_handler_->SetDisabledMediaCodecs({});
    }
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

  if (name.compare(kClientHintHeaders) == 0 && value.IsType<int32>()) {
    if (!persistent_settings_) {
      return false;
    } else {
      persistent_settings_->SetPersistentSetting(
          network::kClientHintHeadersEnabledPersistentSettingsKey,
          std::make_unique<base::Value>(value.AsType<int32>()));
      // Tell NetworkModule (if exists) to re-query persistent settings.
      if (network_module_) {
        network_module_
            ->SetEnableClientHintHeadersFlagsFromPersistentSettings();
      }
      return true;
    }
  }

  if (name.compare(kQUIC) == 0 && value.IsType<int32>()) {
    if (!persistent_settings_) {
      return false;
    } else {
      persistent_settings_->SetPersistentSetting(
          network::kQuicEnabledPersistentSettingsKey,
          std::make_unique<base::Value>(value.AsType<int32>() != 0));
      // Tell NetworkModule (if exists) to re-query persistent settings.
      if (network_module_) {
        network_module_->SetEnableQuicFromPersistentSettings();
      }
      return true;
    }
  }

#if SB_IS(EVERGREEN)
  if (name.compare(kUpdaterMinFreeSpaceBytes) == 0 && value.IsType<int32>()) {
    updater_module_->SetMinFreeSpaceBytes(value.AsType<int32>());
    return true;
  }
#endif
  return false;
}

}  // namespace h5vcc
}  // namespace cobalt
