// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace h5vcc_platform_service {

// Implements the mojom::H5vccPlatformServiceManager interface and extends
// content::DocumentUserData to tie an instance's lifetime to the document /
// RenderFrameHost, with one instance per RenderFrameHost.
class H5vccPlatformServiceManagerImpl
    : public content::DocumentUserData<H5vccPlatformServiceManagerImpl>,
      public mojom::H5vccPlatformServiceManager {
 public:
  // Gets the existing instance for the RFH or creates a new one,
  // and binds the receiver.
  static void GetOrCreate(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver);

  // Not copyable or movable
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
  // No public constructors, to force going through static methods of
  // content::DocumentUserData (e.g., GetOrCreateForCurrentDocument).
  explicit H5vccPlatformServiceManagerImpl(
      content::RenderFrameHost* render_frame_host);

  // Called when a receiver in the set disconnects.
  void OnReceiverDisconnect();

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::ReceiverSet<mojom::H5vccPlatformServiceManager> receivers_;
  base::WeakPtrFactory<H5vccPlatformServiceManagerImpl> weak_factory_{this};
};

}  // namespace h5vcc_platform_service

#endif  // COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_H5VCC_PLATFORM_SERVICE_MANAGER_IMPL_H_
