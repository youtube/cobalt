// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store_android.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "net/android/network_library.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

class TrustStoreAndroid::Impl
    : public base::RefCountedThreadSafe<TrustStoreAndroid::Impl> {
 public:
  explicit Impl(int generation) : generation_(generation) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    std::vector<std::string> roots = net::android::GetUserAddedRoots();

    for (auto& root : roots) {
      bssl::CertErrors errors;
      auto parsed = bssl::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(root),
          net::x509_util::DefaultParseCertificateOptions(), &errors);
      if (!parsed) {
        LOG(ERROR) << "Error parsing certificate:\n" << errors.ToDebugString();
        continue;
      }
      trust_store_.AddTrustAnchor(std::move(parsed));
    }
  }

  // TODO(hchao): see if we can get SyncGetIssueresOf marked const
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) {
    trust_store_.SyncGetIssuersOf(cert, issuers);
  }

  // TODO(hchao): see if we can get GetTrust marked const again
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) {
    return trust_store_.GetTrust(cert);
  }

  int generation() { return generation_; }

 private:
  friend class base::RefCountedThreadSafe<TrustStoreAndroid::Impl>;
  ~Impl() = default;

  // Generation # that trust_store_ was loaded at.
  const int generation_;

  bssl::TrustStoreInMemory trust_store_;
};

TrustStoreAndroid::TrustStoreAndroid() {
  // It's okay for ObserveCertDBChanges to be called on a different sequence
  // than the object was constructed on.
  DETACH_FROM_SEQUENCE(certdb_observer_sequence_checker_);
}

TrustStoreAndroid::~TrustStoreAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(certdb_observer_sequence_checker_);
  if (is_observing_certdb_changes_) {
    CertDatabase::GetInstance()->RemoveObserver(this);
  }
}

void TrustStoreAndroid::Initialize() {
  MaybeInitializeAndGetImpl();
}

// This function is not thread safe. CertDatabase observation is added here
// rather than in the constructor to avoid having to add a TaskEnvironment to
// every unit test that uses TrustStoreAndroid.
void TrustStoreAndroid::ObserveCertDBChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(certdb_observer_sequence_checker_);
  if (!is_observing_certdb_changes_) {
    is_observing_certdb_changes_ = true;
    CertDatabase::GetInstance()->AddObserver(this);
  }
}

void TrustStoreAndroid::OnTrustStoreChanged() {
  // Increment the generation number. This will regenerate the impl_ next time
  // it is fetched. It would be neater to regenerate the impl_ here but
  // complications around blocking of threads prevents this from being easily
  // accomplished.
  generation_++;
}

scoped_refptr<TrustStoreAndroid::Impl>
TrustStoreAndroid::MaybeInitializeAndGetImpl() {
#if BUILDFLAG(IS_COBALT)
  // Allow bypassing the Android trust store (user-added roots) initialization
  // lock by returning the current impl_ if initialization is already in
  // progress. Otherwise, set `is_initializing_ = true` and release the lock so
  // that we can perform the slow constructor work outside of the lock.
  int current_generation;
  {
    base::AutoLock lock(init_lock_);
    current_generation = generation_.load();

    // Return if we already have a fully initialized store for this generation.
    const bool is_impl_available = impl_ && impl_->generation() == current_generation;
    if (is_impl_available) {
      return impl_;
    }
    
    // If another thread is already in the middle of performing the slow
    // background initialization, return the existing implementation. This is a
    // non-blocking bypass: we return what we currently have. This could be nullptr
    // on first init, or the old valid impl_ during a runtime reload to ensure the
    // network thread never stalls.
    if (is_initializing_) {
      return impl_;
    }

    // If we are the first thread to detect that the store needs to be initialized
    // or updated, we take responsibility for the initialization. We set
    // `is_initializing_ = true` to prevent other threads from starting redundant
    // loads, and then release the lock so we can perform the init without
    // blocking others.
    is_initializing_ = true;
  }

  // Perform the slow constructor work (including JNI calls to Android system)
  // OUTSIDE of the lock.
  scoped_refptr<TrustStoreAndroid::Impl> tmp_impl;
  {
    SCOPED_UMA_HISTOGRAM_LONG_TIMER("Net.CertVerifier.AndroidTrustStoreInit");
    tmp_impl = base::MakeRefCounted<TrustStoreAndroid::Impl>(current_generation);
  }

  {
    // Re-acquire the lock to commit the newly initialized store.
    base::AutoLock lock(init_lock_);
    impl_ = tmp_impl;
    is_initializing_ = false;
    return impl_;
  }
#else
  base::AutoLock lock(init_lock_);

  // It is possible that generation_ might be incremented in between the various
  // statements here, but that's okay as the worst case is that we will cause a
  // bit of extra work in reloading the android trust store if we get many
  // OnTrustStoreChanged() calls in rapid succession.
  int current_generation = generation_.load();
  if (!impl_ || impl_->generation() != current_generation) {
    SCOPED_UMA_HISTOGRAM_LONG_TIMER("Net.CertVerifier.AndroidTrustStoreInit");
    impl_ = base::MakeRefCounted<TrustStoreAndroid::Impl>(current_generation);
  }

  return impl_;
#endif  // BUILDFLAG(IS_COBALT)
}

void TrustStoreAndroid::SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                                         bssl::ParsedCertificateList* issuers) {
#if BUILDFLAG(IS_COBALT)
  if (auto impl = MaybeInitializeAndGetImpl()) {
    impl->SyncGetIssuersOf(cert, issuers);
  }
#else
  MaybeInitializeAndGetImpl()->SyncGetIssuersOf(cert, issuers);
#endif  // BUILDFLAG(IS_COBALT)
}

bssl::CertificateTrust TrustStoreAndroid::GetTrust(
    const bssl::ParsedCertificate* cert) {
#if BUILDFLAG(IS_COBALT)
  if (auto impl = MaybeInitializeAndGetImpl()) {
    return impl->GetTrust(cert);
  }
  return bssl::CertificateTrust::ForUnspecified();
#else
  return MaybeInitializeAndGetImpl()->GetTrust(cert);
#endif  // BUILDFLAG(IS_COBALT)
}

std::vector<net::PlatformTrustStore::CertWithTrust>
TrustStoreAndroid::GetAllUserAddedCerts() {
  // TODO(crbug.com/40928765): implement this.
  return {};
}

}  // namespace net
