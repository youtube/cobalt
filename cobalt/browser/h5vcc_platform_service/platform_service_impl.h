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

#ifndef COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_PLATFORM_SERVICE_IMPL_H_
#define COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_PLATFORM_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "starboard/extension/platform_service.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_platform_service {

// Extends DocumentService so that an object's lifetime is scoped to the
// corresponding document / RenderFrameHost (see DocumentService for details).
class PlatformServiceImpl
    : public content::DocumentService<mojom::PlatformService> {
 public:
  // Creates a PlatformServiceImpl.
  // It's bound to the receiver and its lifetime is scoped to the RFH.
  static void Create(
      content::RenderFrameHost& render_frame_host,
      const std::string& service_name,
      mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
      mojo::PendingReceiver<mojom::PlatformService> receiver);

  static void StarboardReceiveMessageCallback(void* context,
                                              const void* data,
                                              uint64_t length);

  PlatformServiceImpl(const PlatformServiceImpl&) = delete;
  PlatformServiceImpl& operator=(const PlatformServiceImpl&) = delete;
  ~PlatformServiceImpl() override;

  // mojom::PlatformService implementation:
  void Send(const std::vector<uint8_t>& data, SendCallback callback) override;

 private:
  PlatformServiceImpl(
      content::RenderFrameHost& render_frame_host,
      const std::string& service_name,
      mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
      mojo::PendingReceiver<mojom::PlatformService> receiver);
  bool OpenStarboardService();
  void OnDataReceivedFromStarboard(const std::vector<uint8_t>& data);

  std::string service_name_;
  mojo::Remote<mojom::PlatformServiceObserver> observer_;

  // This is a handle to the service: a pointer to the private struct.
  CobaltExtensionPlatformService platform_service_ =
      kCobaltExtensionPlatformServiceInvalid;

  base::WeakPtrFactory<PlatformServiceImpl> weak_factory_{this};
};

}  // namespace h5vcc_platform_service

#endif  // COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_PLATFORM_SERVICE_IMPL_H_
