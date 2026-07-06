// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_updater.h"

#if SB_IS(EVERGREEN)
#include "chrome/updater/util.h"
#endif

namespace {

const uint16 kInvalidInstallationIndex = 1000;

}  // namespace

namespace cobalt {
namespace h5vcc {

#if SB_IS(EVERGREEN)
std::string H5vccUpdater::GetUpdaterChannel() const {
  if (!updater_module_) {
    return "";
  }

  return updater_module_->GetUpdaterChannel();
}

void H5vccUpdater::SetUpdaterChannel(const std::string& channel) {
  if (!updater_module_) {
    return;
  }

  // Force an update if SetUpdaterChannel is called, even if
  // the channel doesn't change, as long as it's not
  // "prod"/"experiment"/"control" channel
  if (updater_module_->GetUpdaterChannel().compare(channel) != 0) {
    updater_module_->SetUpdaterChannel(channel);
  } else if (channel == "prod" || channel == "experiment" ||
             channel == "control") {
    return;
  }
  updater_module_->CompareAndSwapForcedUpdate(0, 1);
  updater_module_->RunUpdateCheck();
}

std::string H5vccUpdater::GetUpdateStatus() const {
  if (!updater_module_) {
    return "";
  }

  return updater_module_->GetUpdaterStatus();
}

void H5vccUpdater::ResetInstallations() {
  if (updater_module_) {
    updater_module_->ResetInstallations();
  }
}

uint16 H5vccUpdater::GetInstallationIndex() const {
  if (!updater_module_) {
    return kInvalidInstallationIndex;
  }
  int index = updater_module_->GetInstallationIndex();

  return index == -1 ? kInvalidInstallationIndex : static_cast<uint16>(index);
}

std::string H5vccUpdater::GetLibrarySha256(uint16 index) const {
  return cobalt::updater::GetLibrarySha256(index);
}

bool H5vccUpdater::GetUseCompressedUpdates() const {
  if (!updater_module_) {
    return false;
  }

  return updater_module_->GetUseCompressedUpdates();
}

void H5vccUpdater::SetUseCompressedUpdates(bool use_compressed_updates) {
  if (!updater_module_) {
    return;
  }

  return updater_module_->SetUseCompressedUpdates(use_compressed_updates);
}

bool H5vccUpdater::GetAllowSelfSignedPackages() {
  if (updater_module_) {
    return updater_module_->GetAllowSelfSignedPackages();
  }

  return false;
}

void H5vccUpdater::SetAllowSelfSignedPackages(bool allow_self_signed_packages) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  if (updater_module_) {
    updater_module_->SetAllowSelfSignedPackages(allow_self_signed_packages);
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}

std::string H5vccUpdater::GetUpdateServerUrl() const {
  if (updater_module_) {
    return updater_module_->GetUpdateServerUrl();
  }

  return "";
}

void H5vccUpdater::SetUpdateServerUrl(const std::string& update_server_url) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  if (updater_module_) {
    updater_module_->SetUpdateServerUrl(update_server_url);
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}

bool H5vccUpdater::GetRequireNetworkEncryption() const {
  if (updater_module_) {
    return updater_module_->GetRequireNetworkEncryption();
  }

  return false;
}

void H5vccUpdater::SetRequireNetworkEncryption(
    bool require_network_encryption) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  if (updater_module_) {
    updater_module_->SetRequireNetworkEncryption(require_network_encryption);
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}
#endif  // SB_IS(EVERGREEN)
}  // namespace h5vcc
}  // namespace cobalt
