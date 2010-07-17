// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#if defined(OS_MACOSX)
#include <Security/Security.h>
#elif defined(USE_NSS)
#include <cert.h>
#endif

#include <map>

#include "base/histogram.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/time.h"

namespace net {

namespace {

// Returns true if this cert fingerprint is the null (all zero) fingerprint.
// We use this as a bogus fingerprint value.
bool IsNullFingerprint(const SHA1Fingerprint& fingerprint) {
  for (size_t i = 0; i < arraysize(fingerprint.data); ++i) {
    if (fingerprint.data[i] != 0)
      return false;
  }
  return true;
}

}  // namespace

// static
bool X509Certificate::IsSameOSCert(X509Certificate::OSCertHandle a,
                                   X509Certificate::OSCertHandle b) {
  DCHECK(a && b);
  if (a == b)
    return true;
#if defined(OS_WIN)
  return a->cbCertEncoded == b->cbCertEncoded &&
      memcmp(a->pbCertEncoded, b->pbCertEncoded, a->cbCertEncoded) == 0;
#elif defined(OS_MACOSX)
  if (CFEqual(a, b))
    return true;
  CSSM_DATA a_data, b_data;
  return SecCertificateGetData(a, &a_data) == noErr &&
      SecCertificateGetData(b, &b_data) == noErr &&
      a_data.Length == b_data.Length &&
      memcmp(a_data.Data, b_data.Data, a_data.Length) == 0;
#elif defined(USE_NSS)
  return a->derCert.len == b->derCert.len &&
      memcmp(a->derCert.data, b->derCert.data, a->derCert.len) == 0;
#else
  // TODO(snej): not implemented
  UNREACHED();
  return false;
#endif
}

bool X509Certificate::LessThan::operator()(X509Certificate* lhs,
                                           X509Certificate* rhs) const {
  if (lhs == rhs)
    return false;

  SHA1FingerprintLessThan fingerprint_functor;
  return fingerprint_functor(lhs->fingerprint_, rhs->fingerprint_);
}

// A thread-safe cache for X509Certificate objects.
//
// The cache does not hold a reference to the certificate objects.  The objects
// must |Remove| themselves from the cache upon destruction (or else the cache
// will be holding dead pointers to the objects).
// TODO(rsleevi): There exists a chance of a use-after-free, due to a race
// between Find() and Remove(). See http://crbug.com/49377
class X509Certificate::Cache {
 public:
  static Cache* GetInstance();
  void Insert(X509Certificate* cert);
  void Remove(X509Certificate* cert);
  X509Certificate* Find(const SHA1Fingerprint& fingerprint);

 private:
  typedef std::map<SHA1Fingerprint, X509Certificate*, SHA1FingerprintLessThan>
      CertMap;

  // Obtain an instance of X509Certificate::Cache via GetInstance().
  Cache() {}
  friend struct DefaultSingletonTraits<Cache>;

  // You must acquire this lock before using any private data of this object.
  // You must not block while holding this lock.
  Lock lock_;

  // The certificate cache.  You must acquire |lock_| before using |cache_|.
  CertMap cache_;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};

// Get the singleton object for the cache.
// static
X509Certificate::Cache* X509Certificate::Cache::GetInstance() {
  return Singleton<X509Certificate::Cache>::get();
}

// Insert |cert| into the cache.  The cache does NOT AddRef |cert|.
// Any existing certificate with the same fingerprint will be replaced.
void X509Certificate::Cache::Insert(X509Certificate* cert) {
  AutoLock lock(lock_);

  DCHECK(!IsNullFingerprint(cert->fingerprint())) <<
      "Only insert certs with real fingerprints.";
  cache_[cert->fingerprint()] = cert;
};

// Remove |cert| from the cache.  The cache does not assume that |cert| is
// already in the cache.
void X509Certificate::Cache::Remove(X509Certificate* cert) {
  AutoLock lock(lock_);

  CertMap::iterator pos(cache_.find(cert->fingerprint()));
  if (pos == cache_.end())
    return;  // It is not an error to remove a cert that is not in the cache.
  cache_.erase(pos);
};

// Find a certificate in the cache with the given fingerprint.  If one does
// not exist, this method returns NULL.
X509Certificate* X509Certificate::Cache::Find(
    const SHA1Fingerprint& fingerprint) {
  AutoLock lock(lock_);

  CertMap::iterator pos(cache_.find(fingerprint));
  if (pos == cache_.end())
    return NULL;

  return pos->second;
};

// static
X509Certificate* X509Certificate::CreateFromHandle(
    OSCertHandle cert_handle,
    Source source,
    const OSCertHandles& intermediates) {
  DCHECK(cert_handle);
  DCHECK(source != SOURCE_UNUSED);

  // Check if we already have this certificate in memory.
  X509Certificate::Cache* cache = X509Certificate::Cache::GetInstance();
  X509Certificate* cached_cert =
      cache->Find(CalculateFingerprint(cert_handle));
  if (cached_cert) {
    DCHECK(cached_cert->source_ != SOURCE_UNUSED);
    if (cached_cert->source_ > source ||
        (cached_cert->source_ == source &&
         cached_cert->HasIntermediateCertificates(intermediates))) {
      // Return the certificate with the same fingerprint from our cache.
      DHISTOGRAM_COUNTS("X509CertificateReuseCount", 1);
      return cached_cert;
    }
    // Else the new cert is better and will replace the old one in the cache.
  }

  // Otherwise, allocate and cache a new object.
  X509Certificate* cert = new X509Certificate(cert_handle, source,
                                              intermediates);
  cache->Insert(cert);
  return cert;
}

// static
X509Certificate* X509Certificate::CreateFromBytes(const char* data,
                                                  int length) {
  OSCertHandle cert_handle = CreateOSCertHandleFromBytes(data, length);
  if (!cert_handle)
    return NULL;

  X509Certificate* cert = CreateFromHandle(cert_handle,
                                           SOURCE_LONE_CERT_IMPORT,
                                           OSCertHandles());
  FreeOSCertHandle(cert_handle);
  return cert;
}

X509Certificate::X509Certificate(OSCertHandle cert_handle,
                                 Source source,
                                 const OSCertHandles& intermediates)
    : cert_handle_(DupOSCertHandle(cert_handle)),
      source_(source) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  // Copy/retain the intermediate cert handles.
  for (size_t i = 0; i < intermediates.size(); ++i)
    intermediate_ca_certs_.push_back(DupOSCertHandle(intermediates[i]));
#endif
  // Platform-specific initialization.
  Initialize();
}

X509Certificate::X509Certificate(const std::string& subject,
                                 const std::string& issuer,
                                 base::Time start_date,
                                 base::Time expiration_date)
    : subject_(subject),
      issuer_(issuer),
      valid_start_(start_date),
      valid_expiry_(expiration_date),
      cert_handle_(NULL),
      source_(SOURCE_UNUSED) {
  memset(fingerprint_.data, 0, sizeof(fingerprint_.data));
}

X509Certificate::~X509Certificate() {
  // We might not be in the cache, but it is safe to remove ourselves anyway.
  X509Certificate::Cache::GetInstance()->Remove(this);
  if (cert_handle_)
    FreeOSCertHandle(cert_handle_);
#if defined(OS_MACOSX) || defined(OS_WIN)
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i)
    FreeOSCertHandle(intermediate_ca_certs_[i]);
#endif
}

bool X509Certificate::HasExpired() const {
  return base::Time::Now() > valid_expiry();
}

bool X509Certificate::HasIntermediateCertificate(OSCertHandle cert) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i) {
    if (IsSameOSCert(cert, intermediate_ca_certs_[i]))
      return true;
  }
  return false;
#else
  return true;
#endif
}

bool X509Certificate::HasIntermediateCertificates(const OSCertHandles& certs) {
  for (size_t i = 0; i < certs.size(); ++i) {
    if (!HasIntermediateCertificate(certs[i]))
      return false;
  }
  return true;
}

}  // namespace net
