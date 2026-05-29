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

#ifndef COBALT_BROWSER_H5VCC_UPDATER_H5VCC_UPDATER_IMPL_H_
#define COBALT_BROWSER_H5VCC_UPDATER_H5VCC_UPDATER_IMPL_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/browser/h5vcc_updater/public/mojom/h5vcc_updater.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_updater {

// Implements the H5vccUpdater Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccUpdaterImpl : public content::DocumentService<mojom::H5vccUpdater> {
 public:
  // Creates a H5vccUpdaterImpl. The H5vccUpdaterImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccUpdater> receiver);

  H5vccUpdaterImpl(const H5vccUpdaterImpl&) = delete;
  H5vccUpdaterImpl& operator=(const H5vccUpdaterImpl&) = delete;

  void GetUpdaterChannel(GetUpdaterChannelCallback callback) override;
  void SetUpdaterChannel(const std::string& channel,
                         SetUpdaterChannelCallback callback) override;
  void GetUpdateStatus(GetUpdateStatusCallback callback) override;
  void ResetInstallations(ResetInstallationsCallback callback) override;
  void GetInstallationIndex(GetInstallationIndexCallback callback) override;
  void GetAllowSelfSignedPackages(
      GetAllowSelfSignedPackagesCallback callback) override;
  void SetAllowSelfSignedPackages(
      bool allow_self_signed_packages,
      SetAllowSelfSignedPackagesCallback callback) override;
  void GetUpdateServerUrl(GetUpdateServerUrlCallback callback) override;
  void SetUpdateServerUrl(const std::string& update_server_url,
                          SetUpdateServerUrlCallback callback) override;
  void GetRequireNetworkEncryption(
      GetRequireNetworkEncryptionCallback callback) override;
  void SetRequireNetworkEncryption(
      bool require_network_encryption,
      SetRequireNetworkEncryptionCallback callback) override;
  void GetLibrarySha256(unsigned short index,
                        GetLibrarySha256Callback callback) override;

 private:
  H5vccUpdaterImpl(content::RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<mojom::H5vccUpdater> receiver);
  ~H5vccUpdaterImpl();

  THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_updater

#endif  // COBALT_BROWSER_H5VCC_UPDATER_H5VCC_UPDATER_IMPL_H_
