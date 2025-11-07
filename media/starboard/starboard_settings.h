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

#ifndef MEDIA_STARBOARD_STARBOARD_SETTINGS_H_
#define MEDIA_STARBOARD_STARBOARD_SETTINGS_H_

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <variant>

using H5vccSettingValue = std::variant<std::string, int64_t>;

namespace media {

class StarboardSettings {
public:
 static StarboardSettings* GetInstance();

 void SetSettings(std::map<std::string, H5vccSettingValue> h5vcc_settings);

 std::optional<H5vccSettingValue> GetSetting(const std::string& setting_name);

private:
 StarboardSettings() {}
 ~StarboardSettings() {}

 StarboardSettings(const StarboardSettings&) = delete;
 StarboardSettings& operator=(const StarboardSettings&) = delete;

 std::mutex mutex_;
 
 std::map<std::string, H5vccSettingValue> h5vcc_settings_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_SETTINGS_H_
