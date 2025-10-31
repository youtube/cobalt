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

#ifndef COBALT_BROWSER_H5VCC_SETTINGS_H5VCC_SETTINGS_IMPL_H_
#define COBALT_BROWSER_H5VCC_SETTINGS_H5VCC_SETTINGS_IMPL_H_

#include <string>

#include "cobalt/browser/h5vcc_settings/public/mojom/h5vcc_settings.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_settings {

// Implements the H5vccSettings Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccSettingsImpl
    : public content::DocumentService<mojom::H5vccSettings> {
 public:
  // Creates a H5vccSettingsImpl. The H5vccSettingsImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccSettings> receiver);

  H5vccSettingsImpl(const H5vccSettingsImpl&) = delete;
  H5vccSettingsImpl& operator=(const H5vccSettingsImpl&) = delete;

  void SetValue(const std::string& name,
                mojom::ValuePtr value,
                SetValueCallback callback) override;

 private:
  H5vccSettingsImpl(content::RenderFrameHost& render_frame_host,
                    mojo::PendingReceiver<mojom::H5vccSettings> receiver);
};

}  // namespace h5vcc_settings

#endif  // COBALT_BROWSER_H5VCC_SETTINGS_H5VCC_SETTINGS_IMPL_H_
