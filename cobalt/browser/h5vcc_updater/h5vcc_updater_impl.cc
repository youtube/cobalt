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

#include "cobalt/browser/h5vcc_updater/h5vcc_updater_impl.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "cobalt/updater/util.h"

#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/updater/updater_module.h"
#endif

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/configuration/configuration.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"
#endif

namespace h5vcc_updater {

H5vccUpdaterImpl::H5vccUpdaterImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccUpdater> receiver)
    : content::DocumentService<mojom::H5vccUpdater>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccUpdaterImpl::~H5vccUpdaterImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccUpdaterImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccUpdater> receiver) {
  new H5vccUpdaterImpl(*render_frame_host, std::move(receiver));
}

void H5vccUpdaterImpl::GetUpdaterChannel(GetUpdaterChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (!updater_module) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(updater_module->GetUpdaterChannel());
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::SetUpdaterChannel(const std::string& channel,
                                         SetUpdaterChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (updater_module) {
    // Force an update if SetUpdaterChannel is called, even if
    // the channel doesn't change, as long as it's not
    // "prod"/"experiment"/"control" channel
    if (updater_module->GetUpdaterChannel().compare(channel) != 0) {
      updater_module->SetUpdaterChannel(channel);
    } else if (channel == "prod" || channel == "experiment" ||
               channel == "control" || channel == "rollback") {
      std::move(callback).Run();
      return;
    }
    updater_module->CompareAndSwapForcedUpdate(0, 1);
    updater_module->RunUpdateCheck();
  }
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::GetUpdateStatus(GetUpdateStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (!updater_module) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(updater_module->GetUpdaterStatus());
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::ResetInstallations(ResetInstallationsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (updater_module) {
    updater_module->ResetInstallations();
  }
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::GetInstallationIndex(
    GetInstallationIndexCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  const uint16_t kInvalidInstallationIndex = 1000;
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (!updater_module) {
    std::move(callback).Run(kInvalidInstallationIndex);
    return;
  }
  int index = updater_module->GetInstallationIndex();
  std::move(callback).Run(index == -1 ? kInvalidInstallationIndex
                                      : static_cast<uint16_t>(index));
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::GetAllowSelfSignedPackages(
    GetAllowSelfSignedPackagesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (!updater_module) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(updater_module->GetAllowSelfSignedPackages());
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::SetAllowSelfSignedPackages(
    bool allow_self_signed_packages,
    SetAllowSelfSignedPackagesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD) && ALLOW_EVERGREEN_SIDELOADING
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (updater_module) {
    updater_module->SetAllowSelfSignedPackages(allow_self_signed_packages);
  }
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::GetUpdateServerUrl(GetUpdateServerUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (updater_module) {
    std::string url = updater_module->GetUpdateServerUrl();
    std::move(callback).Run(url);
  } else {
    std::move(callback).Run("");
  }
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::SetUpdateServerUrl(const std::string& update_server_url,
                                          SetUpdateServerUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD) && ALLOW_EVERGREEN_SIDELOADING
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (updater_module) {
    updater_module->SetUpdateServerUrl(update_server_url);
  }
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::GetRequireNetworkEncryption(
    GetRequireNetworkEncryptionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (!updater_module) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(updater_module->GetRequireNetworkEncryption());
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::SetRequireNetworkEncryption(
    bool require_network_encryption,
    SetRequireNetworkEncryptionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD) && ALLOW_EVERGREEN_SIDELOADING
  auto* updater_module = cobalt::updater::UpdaterModule::GetInstance();
  if (updater_module) {
    updater_module->SetRequireNetworkEncryption(require_network_encryption);
  }
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif
}

void H5vccUpdaterImpl::GetLibrarySha256(unsigned short index,
                                        GetLibrarySha256Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  std::move(callback).Run(cobalt::updater::GetLibrarySha256(index));
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace h5vcc_updater
