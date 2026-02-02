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

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "starboard/extension/platform_service.h"
#include "starboard/system.h"

namespace h5vcc_platform_service {

DOCUMENT_USER_DATA_KEY_IMPL(H5vccPlatformServiceManagerImpl);

// static
void H5vccPlatformServiceManagerImpl::GetOrCreate(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccPlatformServiceManager> receiver) {
  if (!render_frame_host || !render_frame_host->IsActive()) {
    return;
  }

  H5vccPlatformServiceManagerImpl* instance =
      H5vccPlatformServiceManagerImpl::GetOrCreateForCurrentDocument(
          render_frame_host);
  if (instance) {
    instance->receivers_.Add(instance, std::move(receiver));
  } else {
    LOG(ERROR) << "Failed to GetOrCreate H5vccPlatformServiceManagerImpl";
  }
}

H5vccPlatformServiceManagerImpl::H5vccPlatformServiceManagerImpl(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<H5vccPlatformServiceManagerImpl>(
          render_frame_host) {
  LOG(INFO) << "H5vccPlatformServiceManagerImpl created for RFH ID: "
            << this->render_frame_host().GetGlobalId();
  receivers_.set_disconnect_handler(base::BindRepeating(
      &H5vccPlatformServiceManagerImpl::OnReceiverDisconnect,
      weak_factory_.GetWeakPtr()));
}

H5vccPlatformServiceManagerImpl::~H5vccPlatformServiceManagerImpl() {
  LOG(INFO) << "H5vccPlatformServiceManagerImpl destroyed for RFH ID: "
            << this->render_frame_host().GetGlobalId();
}

void H5vccPlatformServiceManagerImpl::OnReceiverDisconnect() {
  LOG(INFO) << "A H5vccPlatformServiceManagerImpl receiver disconnected for "
            << "RFH ID: " << render_frame_host().GetGlobalId();
  // DocumentUserData handles object destruction when the RFH is destroyed.
  // No need to delete |this| here.
  // If receivers_.empty(), this instance remains until RFH is destroyed.
}

void H5vccPlatformServiceManagerImpl::Has(const std::string& service_name,
                                          HasCallback callback) {
  auto platform_service_extension =
      static_cast<const CobaltExtensionPlatformServiceApi*>(
          SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
  if (!platform_service_extension) {
    LOG(WARNING) << "The platform service extension is not implemented on this "
                 << "platform";
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
  if (!render_frame_host().IsActive()) {
    LOG(ERROR)
        << "H5vccPlatformServiceManagerImpl::Open: RenderFrameHost not active";
    // Do not run the callback, signaling an error to the [Sync] caller.
    return;
  }

  // Pass the RenderFrameHost& from the base class to
  // PlatformServiceImpl::Create.
  PlatformServiceImpl::Create(render_frame_host(), service_name,
                              std::move(observer), std::move(receiver));

  // Running the callback unblocks the [Sync] call in the renderer.
  std::move(callback).Run();
}

}  // namespace h5vcc_platform_service
