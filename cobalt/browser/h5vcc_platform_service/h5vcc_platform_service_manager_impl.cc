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

#include "cobalt/browser/h5vcc_platform_service/h5vcc_platform_service_manager_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "starboard/extension/platform_service.h"
#include "starboard/system.h"

namespace h5vcc_platform_service {

// static
void H5vccPlatformServiceManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver) {
  if (!render_frame_host || !render_frame_host->IsActive()) {
    return;
  }

  // The instance will delete itself when the connection is dropped, in
  // OnDisconnect.
  new H5vccPlatformServiceManagerImpl(*render_frame_host, std::move(receiver));
}

H5vccPlatformServiceManagerImpl::H5vccPlatformServiceManagerImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver)
    : render_frame_host_(render_frame_host),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &H5vccPlatformServiceManagerImpl::OnDisconnect, base::Unretained(this)));
}

H5vccPlatformServiceManagerImpl::~H5vccPlatformServiceManagerImpl() {
  LOG(INFO) << "H5vccPlatformServiceManagerImpl destroyed for RFH ID: "
            << render_frame_host_.GetGlobalId();
}

void H5vccPlatformServiceManagerImpl::OnDisconnect() {
  LOG(INFO) << "H5vccPlatformServiceManagerImpl disconnected for RFH ID: "
            << render_frame_host_.GetGlobalId();
  delete this;
}

void H5vccPlatformServiceManagerImpl::Has(const std::string& service_name,
                                          HasCallback callback) {
  auto platform_service_extension =
      static_cast<const CobaltExtensionPlatformServiceApi*>(
          SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
  if (!platform_service_extension) {
    LOG(WARNING) << "The extension is not implemented on this platform";
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      platform_service_extension->Has(service_name.c_str()));
}

void H5vccPlatformServiceManagerImpl::Open(
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
    mojo::PendingReceiver<mojom::PlatformService> receiver,
    OpenCallback callback) {
  if (!render_frame_host_.IsActive()) {
    LOG(ERROR)
        << "H5vccPlatformServiceManagerImpl::Open: RenderFrameHost not active";
    return;
  }

  PlatformServiceImpl::Create(render_frame_host_, service_name,
                              std::move(observer), std::move(receiver));
  std::move(callback).Run();
}

}  // namespace h5vcc_platform_service
