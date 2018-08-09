// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/ce_cdm/oemcrypto/mock/src/oemcrypto_engine_mock.h"

#include "starboard/media.h"

namespace {
const OEMCrypto_HDCP_Capability kWidevineMaximumHdcpVersion = HDCP_V1;
}  // namespace

namespace wvoec_mock {

class CryptoEngineXb1 : public CryptoEngine {
 public:
  explicit CryptoEngineXb1(std::auto_ptr<wvcdm::FileSystem> file_system)
      : CryptoEngine(file_system) {}

  OEMCrypto_HDCP_Capability config_current_hdcp_capability() override {
    return HDCP_NONE;
  }

  OEMCrypto_HDCP_Capability config_maximum_hdcp_capability() override {
    return kWidevineMaximumHdcpVersion;
  }

  // Max buffer size for encoded buffer.
  size_t max_buffer_size() override { return 3840 * 2160 * 2; }
};

CryptoEngine* CryptoEngine::MakeCryptoEngine(
    std::auto_ptr<wvcdm::FileSystem> file_system) {
  return new CryptoEngineXb1(file_system);
}

}  // namespace wvoec_mock
