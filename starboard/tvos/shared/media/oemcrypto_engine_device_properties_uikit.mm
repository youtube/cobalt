// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/media/oemcrypto_engine_device_properties_uikit.h"

#include <atomic>

#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/common/time.h"
#include "starboard/media.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"
#include "third_party/internal/ce_cdm/oemcrypto/mock/src/oemcrypto_engine_mock.h"

namespace {

const OEMCrypto_HDCP_Capability kWidevineMaximumHdcpVersion = HDCP_V1;
const int64_t kUnkownHdcpStateTimeoutUsec = 5 * 1000000;

class CryptoEngineDeviceProperties {
 public:
  OEMCrypto_HDCP_Capability getCurrentHdcpCapability() {
    @autoreleasepool {
      id<SBDStarboardApplication> application = SBDGetApplication();
      SBDPlayerManager* playerManager = application.playerManager;
      if (playerManager.hdcpState == kHdcpProtectionStateUnauthenticated) {
        return HDCP_NONE;
      }
      if (playerManager.hdcpState == kHdcpProtectionStateAuthenticated) {
        last_unknown_query_timestamp_ = -1;
        return HDCP_V1;
      }
      // HDCP state is unknown. We allow the player to keep playing for a
      // short time.
      if (last_unknown_query_timestamp_ == -1) {
        last_unknown_query_timestamp_ = starboard::CurrentMonotonicTime();
        return HDCP_V1;
      }
      if (starboard::CurrentMonotonicTime() - last_unknown_query_timestamp_ <=
          kUnkownHdcpStateTimeoutUsec) {
        return HDCP_V1;
      }
      return HDCP_NONE;
    }
  }

  bool getMinimumHdcpRequirment(
      OEMCrypto_HDCP_Capability* min_hdcp_requirment) {
    SB_DCHECK(min_hdcp_requirment);
    if (min_hdcp_requirment && always_enforce_output_protection_) {
      *min_hdcp_requirment = HDCP_V1;
      return true;
    }
    return false;
  }

  void alwaysEnforceOutputProtection() {
    always_enforce_output_protection_ = true;
  }

 private:
  std::atomic_bool always_enforce_output_protection_{false};
  std::atomic_int64_t last_unknown_query_timestamp_{-1};
};

SB_ONCE_INITIALIZE_FUNCTION(CryptoEngineDeviceProperties,
                            GetCryptoEngineDeviceProperties);

}  // namespace

namespace wvoec_mock {

void OEMCrypto_AlwaysEnforceOutputProtection() {
  GetCryptoEngineDeviceProperties()->alwaysEnforceOutputProtection();
}

class CryptoEngineUikit : public CryptoEngine {
 public:
  explicit CryptoEngineUikit(scoped_ptr<FileSystem> file_system)
      : CryptoEngine(file_system) {}

  OEMCrypto_HDCP_Capability config_current_hdcp_capability() override {
    return GetCryptoEngineDeviceProperties()->getCurrentHdcpCapability();
  }

  OEMCrypto_HDCP_Capability config_maximum_hdcp_capability() override {
    return kWidevineMaximumHdcpVersion;
  }

  bool config_minimum_hdcp_requirment(
      OEMCrypto_HDCP_Capability* min_hdcp_requirment) override {
    return GetCryptoEngineDeviceProperties()->getMinimumHdcpRequirment(
        min_hdcp_requirment);
  }

  // Max buffer size for encoded buffer.
  size_t max_buffer_size() override { return 3840 * 2160 * 2; }
};

CryptoEngine* CryptoEngine::MakeCryptoEngine(
    scoped_ptr<FileSystem> file_system) {
  return new CryptoEngineUikit(file_system);
}

}  // namespace wvoec_mock
