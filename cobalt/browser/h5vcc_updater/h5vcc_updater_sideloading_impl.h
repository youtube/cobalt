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

#ifndef COBALT_BROWSER_H5VCC_UPDATER_H5VCC_UPDATER_SIDELOADING_IMPL_H_
#define COBALT_BROWSER_H5VCC_UPDATER_H5VCC_UPDATER_SIDELOADING_IMPL_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_updater {

class H5vccUpdaterSideloadingImpl
    : public content::DocumentService<mojom::H5vccUpdaterSideloading> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccUpdaterSideloading> receiver);

  H5vccUpdaterSideloadingImpl(const H5vccUpdaterSideloadingImpl&) = delete;
  H5vccUpdaterSideloadingImpl& operator=(const H5vccUpdaterSideloadingImpl&) =
      delete;

  void SetAllowSelfSignedPackages(
      bool allow_self_signed_packages,
      SetAllowSelfSignedPackagesCallback callback) override;
  void SetUpdateServerUrl(const std::string& update_server_url,
                          SetUpdateServerUrlCallback callback) override;
  void SetRequireNetworkEncryption(
      bool require_network_encryption,
      SetRequireNetworkEncryptionCallback callback) override;

 private:
  H5vccUpdaterSideloadingImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccUpdaterSideloading> receiver);
  ~H5vccUpdaterSideloadingImpl();

  THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_updater

#endif  // COBALT_BROWSER_H5VCC_UPDATER_H5VCC_UPDATER_SIDELOADING_IMPL_H_
