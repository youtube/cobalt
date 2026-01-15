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

#ifndef COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_H5VCC_PLATFORM_SERVICE_MANAGER_IMPL_H_
#define COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_H5VCC_PLATFORM_SERVICE_MANAGER_IMPL_H_

#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_platform_service {

class H5vccPlatformServiceManagerImpl
    : public mojom::H5vccPlatformServiceManager {
 public:
  // Creates a H5vccPlatformServiceManagerImpl.
  // The lifetime of the created instance is tied to the receiver.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver);

  H5vccPlatformServiceManagerImpl(const H5vccPlatformServiceManagerImpl&) =
      delete;
  H5vccPlatformServiceManagerImpl& operator=(
      const H5vccPlatformServiceManagerImpl&) = delete;
  ~H5vccPlatformServiceManagerImpl() override;

  // mojom::H5vccPlatformServiceManager implementation:
  void Has(const std::string& service_name, HasCallback callback) override;
  void Open(const std::string& service_name,
            mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
            mojo::PendingReceiver<mojom::PlatformService> receiver,
            OpenCallback callback) override;

 private:
  H5vccPlatformServiceManagerImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver);

  // Called when the Mojo connection is lost. Deletes this instance.
  void OnDisconnect();

  content::RenderFrameHost& render_frame_host_;
  mojo::Receiver<mojom::H5vccPlatformServiceManager> receiver_;
};

}  // namespace h5vcc_platform_service

#endif  // COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_H5VCC_PLATFORM_SERVICE_MANAGER_IMPL_H_
