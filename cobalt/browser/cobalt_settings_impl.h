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

#ifndef COBALT_BROWSER_COBALT_SETTINGS_IMPL_H_
#define COBALT_BROWSER_COBALT_SETTINGS_IMPL_H_

#include "cobalt/browser/mojom/cobalt_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace cobalt {

class CobaltSettingsImpl : public mojom::CobaltSettings {
 public:
  CobaltSettingsImpl();
  ~CobaltSettingsImpl() override;

  CobaltSettingsImpl(const CobaltSettingsImpl&) = delete;
  CobaltSettingsImpl& operator=(const CobaltSettingsImpl&) = delete;

  static void Create(mojo::PendingReceiver<mojom::CobaltSettings> receiver);

  // mojom::CobaltSettings implementation.
  void GetSetting(const std::string& key, GetSettingCallback callback) override;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_SETTINGS_IMPL_H_
