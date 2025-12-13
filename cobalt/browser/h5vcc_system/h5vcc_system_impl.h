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

#ifndef COBALT_BROWSER_H5VCC_SYSTEM_H5VCC_SYSTEM_IMPL_H_
#define COBALT_BROWSER_H5VCC_SYSTEM_H5VCC_SYSTEM_IMPL_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/browser/h5vcc_system/public/mojom/h5vcc_system.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_system {

// Implements the H5vccSystem Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccSystemImpl : public content::DocumentService<mojom::H5vccSystem> {
 public:
  // Creates a H5vccSystemImpl. The H5vccSystemImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccSystem> receiver);

  H5vccSystemImpl(const H5vccSystemImpl&) = delete;
  H5vccSystemImpl& operator=(const H5vccSystemImpl&) = delete;

  void GetAdvertisingId(GetAdvertisingIdCallback) override;
  void GetAdvertisingIdSync(GetAdvertisingIdSyncCallback) override;
  void GetLimitAdTracking(GetLimitAdTrackingCallback) override;
  void GetLimitAdTrackingSync(GetLimitAdTrackingSyncCallback) override;
  void GetTrackingAuthorizationStatus(
      GetTrackingAuthorizationStatusCallback) override;
  void GetTrackingAuthorizationStatusSync(
      GetTrackingAuthorizationStatusSyncCallback) override;
  void RequestTrackingAuthorization(
      RequestTrackingAuthorizationCallback) override;
  void GetUserOnExitStrategy(GetUserOnExitStrategyCallback) override;
  void Exit() override;
  void HideSplashScreen() override;

 private:
  H5vccSystemImpl(content::RenderFrameHost& render_frame_host,
                  mojo::PendingReceiver<mojom::H5vccSystem> receiver);
  ~H5vccSystemImpl();

  THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_system

#endif  // COBALT_BROWSER_H5VCC_SYSTEM_H5VCC_SYSTEM_IMPL_H_
