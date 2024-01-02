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

#include "cobalt/media/media_module.h"
#include "cobalt/network/network_module.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/navigator_ua_data.h"

#if SB_IS(EVERGREEN)
#include "cobalt/updater/updater_module.h"
#endif

namespace cobalt {
namespace h5vcc {

// This class can be used to modify system settings from JS.
// Setting enumeration and retrieving are explicitly excluded in the initial
// version to avoid being abused.
class H5vccSettings : public script::Wrappable {
 public:
  typedef base::Callback<bool(const std::string& name, int value)>
      SetSettingFunc;

  typedef script::UnionType2<int32, std::string> SetValueType;

  H5vccSettings(const SetSettingFunc& set_web_setting_func,
                cobalt::media::MediaModule* media_module,
                cobalt::media::CanPlayTypeHandler* can_play_type_handler,
                cobalt::network::NetworkModule* network_module,
#if SB_IS(EVERGREEN)
                cobalt::updater::UpdaterModule* updater_module,
#endif
                web::NavigatorUAData* user_agent_data,
                script::GlobalEnvironment* global_environment,
                persistent_storage::PersistentSettings* persistent_settings);

  // Returns true when the setting is set successfully or if the setting has
  // already been set to the expected value.  Returns false when the setting is
  // invalid or not set to the expected value.
  bool Set(const std::string& name, SetValueType value) const;

  void SetPersistentSettingAsInt(const std::string& key, int value) const;

  int GetPersistentSettingAsInt(const std::string& key,
                                int default_setting) const;

  DEFINE_WRAPPABLE_TYPE(H5vccSettings);

 private:
  const SetSettingFunc set_web_setting_func_;
  cobalt::media::MediaModule* media_module_ = nullptr;
  cobalt::media::CanPlayTypeHandler* can_play_type_handler_ = nullptr;
  cobalt::network::NetworkModule* network_module_ = nullptr;
#if SB_IS(EVERGREEN)
  cobalt::updater::UpdaterModule* updater_module_ = nullptr;
#endif
  web::NavigatorUAData* user_agent_data_;
  script::GlobalEnvironment* global_environment_;
  persistent_storage::PersistentSettings* persistent_settings_;

  DISALLOW_COPY_AND_ASSIGN(H5vccSettings);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_SETTINGS_H_
