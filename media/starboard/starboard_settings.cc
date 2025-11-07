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

#include "media/starboard/starboard_settings.h"
#include "starboard/common/once.h"

namespace media {

SB_ONCE_INITIALIZE_FUNCTION(StarboardSettings,
                            StarboardSettings::GetInstance);

void StarboardSettings::SetSettings(std::map<std::string, H5vccSettingValue> h5vcc_settings) {
    std::lock_guard scoped_lock(mutex_);
    h5vcc_settings_ = h5vcc_settings;
}

std::optional<H5vccSettingValue> StarboardSettings::GetSetting(const std::string& setting_name) {
    std::lock_guard scoped_lock(mutex_);
    auto it = h5vcc_settings_.find(setting_name);

    if (it != h5vcc_settings_.end()) {
        return it->second;
    }

    return std::nullopt;
}

}
