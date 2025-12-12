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

#ifndef NET_CERT_INTERNAL_TRUST_STORE_IN_MEMORY_STARBOARD_H_
#define NET_CERT_INTERNAL_TRUST_STORE_IN_MEMORY_STARBOARD_H_

#include <string_view>
#include <unordered_set>

#include "base/synchronization/lock.h"
#include "net/cert/internal/platform_trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace net {

// Wrapper around TrustStoreInMemory to lazily load trusted root certificates.
class NET_EXPORT TrustStoreInMemoryStarboard : public PlatformTrustStore {
 public:
  TrustStoreInMemoryStarboard();
  ~TrustStoreInMemoryStarboard() override;
  TrustStoreInMemoryStarboard(const TrustStoreInMemoryStarboard&) = delete;
  TrustStoreInMemoryStarboard& operator=(const TrustStoreInMemoryStarboard&) =
      delete;

  // TrustStore implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;

  // net::PlatformTrustStore implementation:
  std::vector<net::PlatformTrustStore::CertWithTrust> GetAllUserAddedCerts()
      override;

  // Returns true if the trust store contains the given ParsedCertificate
  // (matches by DER).
  bool Contains(const bssl::ParsedCertificate* cert) const {
    base::AutoLock scoped_lock(load_mutex_);
    return underlying_trust_store_.Contains(cert);
  }

 private:
  bssl::TrustStoreInMemory underlying_trust_store_;

  // Given a certificate's canonical name, try to load this cert from trusted
  // certs on disk if it is found.
  std::shared_ptr<const bssl::ParsedCertificate> TryLoadCert(
      const std::string_view& cert_name) const;

  // The memory trust store can be accessed by multiple threads, in Chromium,
  // the synchronization issue is solved by initializing trust store at startup
  // and passing constant reference to consumers. Cobalt loads certs lazily and
  // therefore guards the underlying_trust_store_ with mutex.
  mutable base::Lock load_mutex_;

  const std::unordered_set<std::string> trusted_cert_names_on_disk_;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_IN_MEMORY_STARBOARD_H_

