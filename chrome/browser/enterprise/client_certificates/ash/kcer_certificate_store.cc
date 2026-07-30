// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/ash/kcer_certificate_store.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "components/enterprise/client_certificates/core/ash/kcer_private_key.h"
#include "components/enterprise/client_certificates/core/ash/kcer_private_key_factory.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/prefs/pref_service.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

namespace {

// Keys for identity metadata stored in prefs.
constexpr char kSpkiKey[] = "spki";

}  // namespace

KcerCertificateStore::KcerCertificateStore(
    PrefService* pref_service,
    base::WeakPtr<kcer::Kcer> kcer,
    scoped_refptr<base::SequencedTaskRunner> kcer_task_runner)
    : pref_service_(pref_service),
      kcer_(std::move(kcer)),
      kcer_task_runner_(std::move(kcer_task_runner)),
      key_factory_(
          std::make_unique<KcerPrivateKeyFactory>(kcer_, kcer_task_runner_)) {
  CHECK(pref_service_);
  CHECK(kcer_task_runner_);
}

KcerCertificateStore::~KcerCertificateStore() = default;

void KcerCertificateStore::CreatePrivateKey(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
        callback) {
  // Check if an identity already exists with this name.
  const base::DictValue& identity = pref_service_->GetDict(identity_name);
  if (identity.size() && identity.FindString(kSpkiKey)) {
    std::move(callback).Run(base::unexpected(StoreError::kConflictingIdentity));
    return;
  }

  key_factory_->CreatePrivateKey(base::BindOnce(
      &KcerCertificateStore::OnPrivateKeyCreated, weak_factory_.GetWeakPtr(),
      identity_name, std::move(callback)));
}

void KcerCertificateStore::OnPrivateKeyCreated(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)> callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    std::move(callback).Run(base::unexpected(StoreError::kCreateKeyFailed));
    return;
  }

  // Store the SPKI and key source in prefs so we can re-load this key later.
  // The source (kChromeOsHwKey / kChromeOsSwKey) encodes whether the key is
  // hardware-backed.
  std::vector<uint8_t> spki = private_key->GetSubjectPublicKeyInfo();
  base::DictValue identity_metadata;
  identity_metadata.Set(kSpkiKey, base::Base64Encode(spki));
  identity_metadata.Set(kKeySource, static_cast<int>(private_key->GetSource()));
  pref_service_->SetDict(identity_name, std::move(identity_metadata));

  std::move(callback).Run(private_key);
}

void KcerCertificateStore::CommitCertificate(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  if (!certificate) {
    std::move(callback).Run(StoreError::kInvalidCertificateInput);
    return;
  }

  if (!kcer_) {
    std::move(callback).Run(StoreError::kSaveKeyFailed);
    return;
  }

  // Import the certificate into Kcer's user token. The corresponding key
  // must already exist there (created by CreatePrivateKey).
  kcer_->ImportX509Cert(
      kcer::Token::kUser, certificate,
      base::BindOnce(&KcerCertificateStore::OnCertImported,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KcerCertificateStore::OnCertImported(
    base::OnceCallback<void(std::optional<StoreError>)> callback,
    base::expected<void, kcer::Error> result) {
  if (!result.has_value()) {
    // TODO(crbug.com/517117656): Convert this log into a histogram.
    LOG(ERROR) << "Failed to import certificate via Kcer (error: "
               << static_cast<int>(result.error()) << ").";
    std::move(callback).Run(StoreError::kSaveKeyFailed);
    return;
  }
  std::move(callback).Run(std::nullopt);
}

void KcerCertificateStore::CommitIdentity(
    const std::string& temporary_identity_name,
    const std::string& final_identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  if (!pref_service_->HasPrefPath(temporary_identity_name)) {
    std::move(callback).Run(StoreError::kIdentityNotFound);
    return;
  }

  if (!certificate) {
    std::move(callback).Run(StoreError::kInvalidCertificateInput);
    return;
  }

  if (final_identity_name.empty()) {
    std::move(callback).Run(StoreError::kInvalidFinalIdentityName);
    return;
  }

  // Move identity metadata from temporary to permanent location in prefs.
  base::DictValue identity =
      pref_service_->GetDict(temporary_identity_name).Clone();
  pref_service_->SetDict(final_identity_name, std::move(identity));
  pref_service_->ClearPref(temporary_identity_name);

  // Import the certificate into Kcer.
  CommitCertificate(final_identity_name, std::move(certificate),
                    std::move(callback));
}

void KcerCertificateStore::GetIdentity(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback) {
  const base::DictValue& identity = pref_service_->GetDict(identity_name);
  if (identity.empty()) {
    // No identity stored.
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::string* encoded_spki = identity.FindString(kSpkiKey);
  if (!encoded_spki) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Build a dict the factory can parse. `kKey` stays Base64-encoded: the
  // factory's LoadPrivateKeyFromDict() decodes it itself and rejects malformed
  // input, so there is no need to pre-decode here. The persisted source encodes
  // the hardware-vs-software distinction; default to the hardware source when
  // missing, since generated keys attempt hardware-backed first.
  base::DictValue key_dict;
  key_dict.Set(kKey, *encoded_spki);
  key_dict.Set(
      kKeySource,
      identity.FindInt(kKeySource)
          .value_or(static_cast<int>(PrivateKeySource::kChromeOsHwKey)));

  key_factory_->LoadPrivateKeyFromDict(
      key_dict, base::BindOnce(&KcerCertificateStore::OnIdentityKeyLoaded,
                               weak_factory_.GetWeakPtr(), identity_name,
                               std::move(callback)));
}

void KcerCertificateStore::OnIdentityKeyLoaded(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Key no longer exists in Kcer (may have been removed externally).
    std::move(callback).Run(base::unexpected(StoreError::kLoadKeyFailed));
    return;
  }

  // The factory already matched this key against Kcer's certs and bound the
  // matching one (if any) while loading the key, so reuse that instead of
  // listing the certs a second time. The cert is null until one is committed.
  scoped_refptr<net::X509Certificate> certificate = private_key->GetBoundCert();
  std::move(callback).Run(ClientIdentity(
      identity_name, std::move(private_key), std::move(certificate)));
}

void KcerCertificateStore::DeleteIdentities(
    const std::vector<std::string>& identity_names,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  for (const std::string& identity_name : identity_names) {
    if (identity_name.empty()) {
      std::move(callback).Run(StoreError::kInvalidIdentityName);
      return;
    }
  }

  // Remove keys and certificates from Kcer, then clear prefs metadata.
  for (const std::string& identity_name : identity_names) {
    const base::DictValue& identity = pref_service_->GetDict(identity_name);
    const std::string* encoded_spki = identity.FindString(kSpkiKey);

    // Decode the stored SPKI into an owned copy before clearing the pref below,
    // since `encoded_spki` points into the pref-owned dict.
    std::string decoded_spki;
    const bool has_key = encoded_spki && kcer_ &&
                         base::Base64Decode(*encoded_spki, &decoded_spki);

    // The pref entry is always cleared, even when there is no key to remove or
    // the stored SPKI fails to decode.
    pref_service_->ClearPref(identity_name);
    if (!has_key) {
      continue;
    }

    // Best-effort removal from Kcer.
    kcer::PublicKeySpki spki(
        std::vector<uint8_t>(decoded_spki.begin(), decoded_spki.end()));
    kcer_->RemoveKeyAndCerts(
        kcer::PrivateKeyHandle(kcer::Token::kUser, std::move(spki)),
        base::BindOnce([](base::expected<void, kcer::Error> result) {
          if (!result.has_value()) {
            // TODO(crbug.com/517117656): Convert this log into a histogram.
            LOG(WARNING) << "Failed to remove key/certs from Kcer (error: "
                         << static_cast<int>(result.error()) << ").";
          }
        }));
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace client_certificates
