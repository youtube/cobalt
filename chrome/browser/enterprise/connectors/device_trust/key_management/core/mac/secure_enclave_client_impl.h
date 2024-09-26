// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_IMPL_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_helper.h"

namespace enterprise_connectors {

// Uses Apple APIs to interact with the Secure Enclave and
// perform key operations.
class SecureEnclaveClientImpl : public SecureEnclaveClient {
 public:
  SecureEnclaveClientImpl();
  ~SecureEnclaveClientImpl() override;

  // SecureEnclaveClient:
  base::ScopedCFTypeRef<SecKeyRef> CreatePermanentKey() override;
  base::ScopedCFTypeRef<SecKeyRef> CopyStoredKey(KeyType type) override;
  bool UpdateStoredKeyLabel(KeyType current_key_type,
                            KeyType new_key_type) override;
  bool DeleteKey(KeyType type) override;
  bool GetStoredKeyLabel(KeyType type, std::vector<uint8_t>& output) override;
  bool ExportPublicKey(SecKeyRef key, std::vector<uint8_t>& output) override;
  bool SignDataWithKey(SecKeyRef key,
                       base::span<const uint8_t> data,
                       std::vector<uint8_t>& output) override;
  bool VerifySecureEnclaveSupported() override;

 private:
  std::unique_ptr<SecureEnclaveHelper> helper_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_CLIENT_IMPL_H_
