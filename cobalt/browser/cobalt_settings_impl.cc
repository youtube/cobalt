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

#include "cobalt/browser/cobalt_settings_impl.h"

#include "cobalt/browser/global_features.h"

namespace cobalt {

CobaltSettingsImpl::CobaltSettingsImpl() = default;

CobaltSettingsImpl::~CobaltSettingsImpl() = default;

void CobaltSettingsImpl::GetSetting(const std::string& key,
                                    GetSettingCallback callback) {
  auto setting = GlobalFeatures::GetInstance()->GetSetting(key);
  if (!setting) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (std::holds_alternative<std::string>(*setting)) {
    std::move(callback).Run(
        mojom::SettingValue::NewStringValue(std::get<std::string>(*setting)));
  } else {
    std::move(callback).Run(
        mojom::SettingValue::NewIntValue(std::get<int64_t>(*setting)));
  }
}

}  // namespace cobalt
