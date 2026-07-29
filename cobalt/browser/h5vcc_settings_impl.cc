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

#include "cobalt/browser/h5vcc_settings_impl.h"

#include <memory>

#include "base/notreached.h"
#include "cobalt/browser/global_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace cobalt {

H5vccSettingsImpl::H5vccSettingsImpl() = default;

H5vccSettingsImpl::~H5vccSettingsImpl() = default;

// static
void H5vccSettingsImpl::Create(
    mojo::PendingReceiver<mojom::H5vccSettings> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<H5vccSettingsImpl>(),
                              std::move(receiver));
}

void H5vccSettingsImpl::GetSettings(GetSettingsCallback callback) {
  auto settings = GlobalFeatures::GetInstance()->GetSettings();
  auto mojom_settings = mojom::Settings::New();
  for (const auto& [key, value] : settings) {
    std::visit(
        [&mojom_settings, &key](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, std::string>) {
            mojom_settings->settings[key] =
                mojom::SettingValue::NewStringValue(arg);
          } else if constexpr (std::is_same_v<T, int64_t>) {
            mojom_settings->settings[key] =
                mojom::SettingValue::NewIntValue(arg);
          } else {
            NOTREACHED();
          }
        },
        value);
  }
  std::move(callback).Run(std::move(mojom_settings));
}

}  // namespace cobalt
