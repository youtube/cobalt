// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ash/kcer_private_key.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "chromeos/ash/components/kcer/ssl_private_key_kcer.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

// Invokes Kcer::Sign on the Kcer sequence. Used by SignSlowly to bridge from
// a worker thread to the UI thread.
void SignOnKcerSequence(
    base::WeakPtr<kcer::Kcer> kcer,
    kcer::PublicKeySpki spki,
    std::vector<uint8_t> data,
    base::OnceCallback<void(base::expected<kcer::Signature, kcer::Error>)>
        callback) {
  if (!kcer) {
    std::move(callback).Run(
        base::unexpected(kcer::Error::kTokenIsNotAvailable));
    return;
  }
  // Rebuild a handle from the SPKI on the user token. Managed client-cert keys
  // are always generated and looked up on kcer::Token::kUser (see
  // KcerPrivateKeyFactory), we scope the handle to that token to stay
  // consistent with the rest of the load/generate flow.
  kcer->Sign(kcer::PrivateKeyHandle(kcer::Token::kUser, std::move(spki)),
             kcer::SigningScheme::kEcdsaSecp256r1Sha256,
             kcer::DataToSign(std::move(data)), std::move(callback));
}

// Receives the sign result on the UI thread, stores it, and signals the
// WaitableEvent so the blocked worker thread can proceed.
void OnSignedForSlowSign(
    base::WaitableEvent* event,
    std::optional<std::vector<uint8_t>>* out_result,
    base::expected<kcer::Signature, kcer::Error> sign_result) {
  if (sign_result.has_value()) {
    *out_result = sign_result->value();
  } else {
    LOG(ERROR) << "Kcer signing failed with error: "
               << static_cast<int>(sign_result.error());
  }
  event->Signal();
}

}  // namespace

KcerPrivateKey::KcerPrivateKey(
    base::WeakPtr<kcer::Kcer> kcer,
    kcer::PublicKeySpki spki,
    scoped_refptr<base::SequencedTaskRunner> kcer_task_runner,
    PrivateKeySource source)
    : PrivateKey(source, /*ssl_private_key=*/nullptr),
      kcer_(std::move(kcer)),
      spki_(std::move(spki)),
      kcer_task_runner_(std::move(kcer_task_runner)) {
  CHECK(source == PrivateKeySource::kChromeOsHwKey ||
        source == PrivateKeySource::kChromeOsSwKey);
  CHECK(kcer_task_runner_);
  // GetSubjectPublicKeyInfo(), ToProto() and ToDict() return the SPKI, and
  // signing rebuilds a handle from it, so it must be non-empty.
  CHECK(!spki_.value().empty());
}

KcerPrivateKey::~KcerPrivateKey() = default;

void KcerPrivateKey::BindCert(scoped_refptr<const kcer::Cert> cert,
                              kcer::KeyInfo key_info) {
  CHECK(cert);
  // Retain the X509 cert so GetBoundCert() can hand it to the certificate
  // store, which would otherwise re-list Kcer's certs to recover it.
  bound_cert_ = cert->GetX509Cert();
  // A Kcer-managed cert and its KeyInfo are available, build the TLS
  // surface and hand it to the base class. GetSSLPrivateKey() returns this.
  // Before BindCert(), `ssl_private_key_` is nullptr, so the TLS
  // surface is unusable; SignSlowly() (used for CSR upload) still works because
  // it only needs the PublicKey. The KeyInfo is consumed here and not retained:
  // only `ssl_private_key_` needs it.
  ssl_private_key_ = base::MakeRefCounted<kcer::SSLPrivateKeyKcer>(
      kcer_, std::move(cert), key_info.key_type,
      base::flat_set<kcer::SigningScheme>(
          key_info.supported_signing_schemes.begin(),
          key_info.supported_signing_schemes.end()));
}

scoped_refptr<net::X509Certificate> KcerPrivateKey::GetBoundCert() const {
  return bound_cert_;
}

std::optional<std::vector<uint8_t>> KcerPrivateKey::SignSlowly(
    base::span<const uint8_t> data) const {
  // SignSlowly is called on a worker thread (from KeyUploadClient via
  // ThreadPool::PostTaskAndReplyWithResult). Kcer operations must run on the
  // UI thread. We post the sign request there and wait for the result.
  //
  // TODO(b/524698801): This back-and-forth (worker thread -> UI thread -> block
  // on WaitableEvent) exists only because KeyUploadClient always posts signing
  // to a worker thread. Once the posting decision is delegated to the PrivateKey
  // implementation, this variant should sign directly on the Kcer/UI sequence.
  CHECK(!kcer_task_runner_->RunsTasksInCurrentSequence());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::optional<std::vector<uint8_t>> result;
  kcer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SignOnKcerSequence, kcer_, spki_,
                                std::vector<uint8_t>(data.begin(), data.end()),
                                base::BindOnce(&OnSignedForSlowSign,
                                               base::Unretained(&event),
                                               base::Unretained(&result))));
  // Blocking on the WaitableEvent is the whole point of SignSlowly(); allow it
  // at the call site rather than via the deprecated WithBaseSyncPrimitives
  // task trait. KcerPrivateKey is friended in thread_restrictions.h.
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  event.Wait();
  return result;
}

std::vector<uint8_t> KcerPrivateKey::GetSubjectPublicKeyInfo() const {
  return spki_.value();
}

crypto::SignatureVerifier::SignatureAlgorithm KcerPrivateKey::GetAlgorithm()
    const {
  return crypto::SignatureVerifier::ECDSA_SHA256;
}

client_certificates_pb::PrivateKey KcerPrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(source_));
  // Store the SPKI bytes so the key can be re-loaded from Kcer later.
  const std::vector<uint8_t>& spki = spki_.value();
  private_key.set_wrapped_key(std::string(spki.begin(), spki.end()));
  // Hardware-backed state is encoded in `source_`, set above.
  return private_key;
}

base::DictValue KcerPrivateKey::ToDict() const {
  // Hardware-backed state is encoded in the source written by
  // BuildSerializedPrivateKey().
  return BuildSerializedPrivateKey(spki_.value());
}

}  // namespace client_certificates
