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

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "cobalt/browser/h5vcc_platform_service/public/mojom/h5vcc_platform_service.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "starboard/extension/platform_service.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_platform_service {

// Status of a PlatformService::Send() operation for UMA telemetry.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PlatformServiceSendStatus {
  kSuccess = 0,
  kMessageTooLarge = 1,
  kExtensionNotImplemented = 2,
  kInvalidServiceHandle = 3,
  kStarboardInvalidState = 4,
  kNullResponseData = 5,
  kMaxValue = kNullResponseData,
};

// Implements mojom::PlatformService on either the UI thread (scoped to the
// document via DocumentService) or the dedicated platform service thread
// (scoped to the document's OffUiThreadPlatformServiceManager).
class PlatformServiceImpl : public mojom::PlatformService {
 public:
  // Creates a PlatformServiceImpl on the UI thread whose lifetime is scoped to
  // the RFH.
  static void Create(
      content::RenderFrameHost& render_frame_host,
      const std::string& service_name,
      mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
      mojo::PendingReceiver<mojom::PlatformService> receiver);

  // Creates a PlatformServiceImpl on the platform service thread. Returns
  // nullptr if opening the underlying Starboard service fails.
  static std::unique_ptr<PlatformServiceImpl> CreateOffUiThread(
      const std::string& service_name,
      mojo::PendingRemote<mojom::PlatformServiceObserver> observer);

  static void StarboardReceiveMessageCallback(void* context,
                                              const void* data,
                                              uint64_t length);

  static const CobaltExtensionPlatformServiceApi* GetPlatformServiceApi();
  static void SetPlatformServiceApiForTesting(
      const CobaltExtensionPlatformServiceApi* api);

  PlatformServiceImpl(const PlatformServiceImpl&) = delete;
  PlatformServiceImpl& operator=(const PlatformServiceImpl&) = delete;
  ~PlatformServiceImpl() override;

  // mojom::PlatformService implementation:
  void Send(base::span<const uint8_t> data, SendCallback callback) override;

 private:
  PlatformServiceImpl(
      content::RenderFrameHost* render_frame_host,
      const std::string& service_name,
      mojo::PendingRemote<mojom::PlatformServiceObserver> observer);
  bool OpenStarboardService();
  void OnDataReceivedFromStarboard(const std::vector<uint8_t>& data);

  // Non-null only when created on the UI thread via Create().
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  std::string service_name_;
  mojo::Remote<mojom::PlatformServiceObserver> observer_;

  // This is a handle to the service: a pointer to the private struct.
  CobaltExtensionPlatformService platform_service_ =
      kCobaltExtensionPlatformServiceInvalid;

  base::RepeatingCallback<void(const std::vector<uint8_t>&)>
      on_data_received_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PlatformServiceImpl> weak_factory_{this};
};

}  // namespace h5vcc_platform_service

#endif  // COBALT_BROWSER_H5VCC_PLATFORM_SERVICE_PLATFORM_SERVICE_IMPL_H_
