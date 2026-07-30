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

#include "cobalt/browser/h5vcc_settings/h5vcc_settings_impl.h"

#include <string>
#include <variant>

#include "cobalt/browser/global_features.h"

namespace h5vcc_settings {

// static
void H5vccSettingsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSettings> receiver) {
  new H5vccSettingsImpl(*render_frame_host, std::move(receiver));
}

H5vccSettingsImpl::H5vccSettingsImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSettings> receiver)
    : content::DocumentService<mojom::H5vccSettings>(render_frame_host,
                                                     std::move(receiver)) {}

void H5vccSettingsImpl::SetValue(const std::string& name,
                                 mojom::ValuePtr value,
                                 SetValueCallback callback) {
  if (!value) {
    LOG(WARNING) << "H5vccSettings::SetValue received a null value for '"
                 << name << "'.";
    std::move(callback).Run();
    return;
  }

  auto* global_features = cobalt::GlobalFeatures::GetInstance();
  CHECK(global_features);

  cobalt::GlobalFeatures::SettingValue setting_value;
  switch (value->which()) {
    case mojom::Value::Tag::kStringValue:
      setting_value = value->get_string_value();
      break;
    case mojom::Value::Tag::kIntValue:
      setting_value = value->get_int_value();
      break;
    default:
      LOG(WARNING) << "H5vccSettings::SetValue received an unknown value "
                      "type for '"
                   << name << "'.";
      std::move(callback).Run();
      return;
  }
  global_features->SetSettings(name, setting_value);
  std::move(callback).Run();
}

}  // namespace h5vcc_settings
