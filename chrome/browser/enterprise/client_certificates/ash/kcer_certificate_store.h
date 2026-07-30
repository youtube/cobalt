// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_ASH_KCER_CERTIFICATE_STORE_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_ASH_KCER_CERTIFICATE_STORE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/store_error.h"

class PrefService;

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

class KcerPrivateKeyFactory;
class PrivateKey;

// CertificateStore implementation for ChromeOS that stores keys and
// certificates via Kcer (ChromeOS key/certificate manager). Keys are generated
// in the user's PKCS#11 slot (TPM-backed when available). Certificates are
// imported into the user token's NSS database via Kcer, making them
// automatically visible in chrome://settings/certificates and to
// ClientCertStoreKcer for TLS client auth.
//
// Identity metadata (name -> SPKI mapping) is stored in PrefService for
// persistence across restarts.
class KcerCertificateStore : public CertificateStore {
 public:
  KcerCertificateStore(
      PrefService* pref_service,
      base::WeakPtr<kcer::Kcer> kcer,
      scoped_refptr<base::SequencedTaskRunner> kcer_task_runner);
  ~KcerCertificateStore() override;

  // CertificateStore:
  void CreatePrivateKey(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback) override;
  void CommitCertificate(
      const std::string& identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback) override;
  void CommitIdentity(
      const std::string& temporary_identity_name,
      const std::string& final_identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback) override;
  void GetIdentity(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
          callback) override;
  void DeleteIdentities(
      const std::vector<std::string>& identity_names,
      base::OnceCallback<void(std::optional<StoreError>)> callback) override;

 private:
  void OnPrivateKeyCreated(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback,
      scoped_refptr<PrivateKey> private_key);

  void OnCertImported(
      base::OnceCallback<void(std::optional<StoreError>)> callback,
      base::expected<void, kcer::Error> result);

  void OnIdentityKeyLoaded(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
          callback,
      scoped_refptr<PrivateKey> private_key);

  raw_ptr<PrefService> pref_service_;
  base::WeakPtr<kcer::Kcer> kcer_;
  scoped_refptr<base::SequencedTaskRunner> kcer_task_runner_;
  std::unique_ptr<KcerPrivateKeyFactory> key_factory_;

  base::WeakPtrFactory<KcerCertificateStore> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_ASH_KCER_CERTIFICATE_STORE_H_
