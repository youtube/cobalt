//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0//
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

#include <memory>
#include <vector>
#include <cstring>

#include "starboard/file.h"
#include "starboard/system.h"

#include "third_party/starboard/rdk/shared/hang_detector.h"
#include "third_party/starboard/rdk/shared/log_override.h"

#if defined(HAS_CRYPTOGRAPHY)
#include <cryptography/cryptography.h>
#if defined(HAS_RFC_API)
#include <rfcapi.h>
#endif
#endif

namespace {

template<typename T>
struct RefDeleter {
  void operator()(T* ref) { if ( ref ) ref->Release(); }
};

template<typename T>
using ScopedRef = std::unique_ptr<T, RefDeleter<T>>;

}

bool SbSystemSignWithCertificationSecretKey(const uint8_t* message,
                                            size_t message_size_in_bytes,
                                            uint8_t* digest,
                                            size_t digest_size_in_bytes) {
  bool result = false;

#if defined(HAS_CRYPTOGRAPHY)

#if (THUNDER_VERSION_MAJOR > 4 || (THUNDER_VERSION_MAJOR == 4 && THUNDER_VERSION_MINOR >= 4))
  using namespace WPEFramework::Exchange;
#else
  using namespace WPEFramework::Cryptography;
  using CryptographyVault = cryptographyvault;
#endif

  third_party::starboard::rdk::shared::HangMonitor hang_monitor(__func__);

  std::string key_name;
  const char *env = std::getenv("COBALT_CERT_KEY_NAME");
  if ( env != nullptr ) {
    key_name = env;
    SB_LOG(INFO) << "Using ENV set key name: '" << key_name << "'";
  } else {
#if defined(HAS_RFC_API)
    const char kRFCParamName[] = "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Cobalt.AuthCertKeyName";
    char *callerId = strdup("Cobalt");
    RFC_ParamData_t param;
    memset(&param, 0, sizeof (param));
    WDMP_STATUS status = getRFCParameter(callerId, kRFCParamName, &param);
    if ( status == WDMP_SUCCESS && param.type == WDMP_STRING ) {
      key_name = param.value;
      SB_LOG(INFO) << "Using RFC provided key name: '" << key_name << "'";
    }
    free(callerId);
#endif
  }

  if ( key_name.empty() ) {
    const char kDefaultKeyName[] = "0381000003810001.key";
    key_name = kDefaultKeyName;
    SB_LOG(INFO) << "Using default key name: '" << key_name << "'";
  }

  const char *env_connection_point = std::getenv("COBALT_ICRYPTO_ACCESS_POINT");
  std::string connection_point = env_connection_point ? env_connection_point : EMPTY_STRING;

  if (env_connection_point != nullptr) {
    struct stat info;
    if (stat(env_connection_point, &info) == 0)
      connection_point = env_connection_point;
    else
      SB_LOG(WARNING) << "ICrypto connection point ('" << env_connection_point << "') does not exist.";
  }

  if (connection_point.empty())
    SB_LOG(INFO) << "Using ICrypto directly.";
  else
    SB_LOG(INFO) << "Using ICrypto connection point: '" << connection_point << "'.";

  ScopedRef<ICryptography> icrypto;
  ScopedRef<IVault> vault;
  ScopedRef<IPersistent> persistent;
  ScopedRef<IHash> hash;

  icrypto.reset( ICryptography::Instance(connection_point) );
  if ( !icrypto ) {
    SB_LOG(ERROR) << "Failed to create ICryptography instance";
    return false;
  }
  vault.reset( icrypto->Vault(CryptographyVault::CRYPTOGRAPHY_VAULT_DEFAULT) );

  if ( !vault ) {
    SB_LOG(ERROR) << "Failed to get default vault";
    return false;
  }

  persistent.reset( vault->QueryInterface<IPersistent>() );

  if ( !persistent ) {
    SB_LOG(ERROR) << "IPersistent is not implemented";
    return false;
  }

  uint32_t rc, key_id = 0;
  if ( (rc = persistent->Load(key_name, key_id)) != WPEFramework::Core::ERROR_NONE ) {
    SB_LOG(ERROR) << "Failed to load key: '" << key_name << "' rc: " << rc;
    persistent->Flush();
    return false;
  } else {
    SB_LOG(INFO) << "Loaded key id: 0x" << std::hex << key_id;
    hash.reset( vault->HMAC(hashtype::SHA256, key_id) );
  }

  if ( !hash ) {
    SB_LOG(ERROR) << "Vault returned null HMAC for key id: 0x" << std::hex << key_id;
  }
  else if ( (rc = hash->Ingest( message_size_in_bytes, message )) != message_size_in_bytes ) {
    SB_LOG(ERROR) << "HMAC 'Ingest' failed, rc: " << rc << " message size: " << message_size_in_bytes;
  }
  else if ( (rc = hash->Calculate( digest_size_in_bytes, digest )) != digest_size_in_bytes ) {
    SB_LOG(ERROR) << "HMAC 'Calculate' failed, rc: " << rc << " digest size: " << digest_size_in_bytes;
  }
  else {
    SB_LOG(INFO) << "Successfully signed cert scope message";
    result = true;
  }

  hash.reset();

  if ( (rc = persistent->Flush()) != WPEFramework::Core::ERROR_NONE ) {
    SB_LOG(ERROR) << "Failed to flush persistent vault, rc: " << rc;
  }
#endif

  return result;
}
