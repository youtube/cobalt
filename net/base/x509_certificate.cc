// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "net/base/pem_tokenizer.h"

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

// Indicates the order to use when trying to decode binary data, which is
// based on (speculation) as to what will be most common -> least common
const X509Certificate::Format kFormatDecodePriority[] = {
  X509Certificate::FORMAT_SINGLE_CERTIFICATE,
  X509Certificate::FORMAT_PKCS7
};

// The PEM block header used for DER certificates
const char kCertificateHeader[] = "CERTIFICATE";
// The PEM block header used for PKCS#7 data
const char kPKCS7Header[] = "PKCS7";

// A thread-safe cache for X509Certificate objects.
//
// The cache does not hold a reference to the certificate objects.  The objects
// must |Remove| themselves from the cache upon destruction (or else the cache
// will be holding dead pointers to the objects).
// TODO(rsleevi): There exists a chance of a use-after-free, due to a race
// between Find() and Remove(). See http://crbug.com/49377
class X509CertificateCache {
 public:
  void Insert(X509Certificate* cert);
  void Remove(X509Certificate* cert);
  X509Certificate* Find(const SHA1Fingerprint& fingerprint);

 private:
  typedef std::map<SHA1Fingerprint, X509Certificate*, SHA1FingerprintLessThan>
      CertMap;

  // Obtain an instance of X509CertificateCache via a LazyInstance.
  X509CertificateCache() {}
  ~X509CertificateCache() {}
  friend struct base::DefaultLazyInstanceTraits<X509CertificateCache>;

  // You must acquire this lock before using any private data of this object.
  // You must not block while holding this lock.
  base::Lock lock_;

  // The certificate cache.  You must acquire |lock_| before using |cache_|.
  CertMap cache_;

  DISALLOW_COPY_AND_ASSIGN(X509CertificateCache);
};

base::LazyInstance<X509CertificateCache,
                   base::LeakyLazyInstanceTraits<X509CertificateCache> >
    g_x509_certificate_cache(base::LINKER_INITIALIZED);

// Insert |cert| into the cache.  The cache does NOT AddRef |cert|.
// Any existing certificate with the same fingerprint will be replaced.
void X509CertificateCache::Insert(X509Certificate* cert) {
  base::AutoLock lock(lock_);

  DCHECK(!IsNullFingerprint(cert->fingerprint())) <<
      "Only insert certs with real fingerprints.";
  cache_[cert->fingerprint()] = cert;
};

// Remove |cert| from the cache.  The cache does not assume that |cert| is
// already in the cache.
void X509CertificateCache::Remove(X509Certificate* cert) {
  base::AutoLock lock(lock_);

  CertMap::iterator pos(cache_.find(cert->fingerprint()));
  if (pos == cache_.end())
    return;  // It is not an error to remove a cert that is not in the cache.
  cache_.erase(pos);
};

// Find a certificate in the cache with the given fingerprint.  If one does
// not exist, this method returns NULL.
X509Certificate* X509CertificateCache::Find(
    const SHA1Fingerprint& fingerprint) {
  base::AutoLock lock(lock_);

  CertMap::iterator pos(cache_.find(fingerprint));
  if (pos == cache_.end())
    return NULL;

  return pos->second;
};

}  // namespace

bool X509Certificate::LessThan::operator()(X509Certificate* lhs,
                                           X509Certificate* rhs) const {
  if (lhs == rhs)
    return false;

  SHA1FingerprintLessThan fingerprint_functor;
  return fingerprint_functor(lhs->fingerprint_, rhs->fingerprint_);
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

// static
X509Certificate* X509Certificate::CreateFromHandle(
    OSCertHandle cert_handle,
    Source source,
    const OSCertHandles& intermediates) {
  DCHECK(cert_handle);
  DCHECK(source != SOURCE_UNUSED);

  // Check if we already have this certificate in memory.
  X509CertificateCache* cache = g_x509_certificate_cache.Pointer();
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

#if defined(OS_WIN)
static X509Certificate::OSCertHandle CreateOSCert(base::StringPiece der_cert) {
  X509Certificate::OSCertHandle cert_handle = NULL;
  BOOL ok = CertAddEncodedCertificateToStore(
      X509Certificate::cert_store(), X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      reinterpret_cast<const BYTE*>(der_cert.data()), der_cert.size(),
      CERT_STORE_ADD_USE_EXISTING, &cert_handle);
  return ok ? cert_handle : NULL;
}
#else
static X509Certificate::OSCertHandle CreateOSCert(base::StringPiece der_cert) {
  return X509Certificate::CreateOSCertHandleFromBytes(
      const_cast<char*>(der_cert.data()), der_cert.size());
}
#endif

// static
X509Certificate* X509Certificate::CreateFromDERCertChain(
    const std::vector<base::StringPiece>& der_certs) {
  if (der_certs.size() == 0)
    return NULL;

  X509Certificate::OSCertHandles intermediate_ca_certs;
  for (size_t i = 1; i < der_certs.size(); i++) {
    OSCertHandle handle = CreateOSCert(der_certs[i]);
    DCHECK(handle);
    intermediate_ca_certs.push_back(handle);
  }

  OSCertHandle handle = CreateOSCert(der_certs[0]);
  DCHECK(handle);
  X509Certificate* cert =
      CreateFromHandle(handle, SOURCE_FROM_NETWORK, intermediate_ca_certs);
  FreeOSCertHandle(handle);
  for (size_t i = 0; i < intermediate_ca_certs.size(); i++)
    FreeOSCertHandle(intermediate_ca_certs[i]);

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

CertificateList X509Certificate::CreateCertificateListFromBytes(
    const char* data, int length, int format) {
  OSCertHandles certificates;

  // Check to see if it is in a PEM-encoded form. This check is performed
  // first, as both OS X and NSS will both try to convert if they detect
  // PEM encoding, except they don't do it consistently between the two.
  base::StringPiece data_string(data, length);
  std::vector<std::string> pem_headers;

  // To maintain compatibility with NSS/Firefox, CERTIFICATE is a universally
  // valid PEM block header for any format.
  pem_headers.push_back(kCertificateHeader);
  if (format & FORMAT_PKCS7)
    pem_headers.push_back(kPKCS7Header);

  PEMTokenizer pem_tok(data_string, pem_headers);
  while (pem_tok.GetNext()) {
    std::string decoded(pem_tok.data());

    OSCertHandle handle = NULL;
    if (format & FORMAT_PEM_CERT_SEQUENCE)
      handle = CreateOSCertHandleFromBytes(decoded.c_str(), decoded.size());
    if (handle != NULL) {
      // Parsed a DER encoded certificate. All PEM blocks that follow must
      // also be DER encoded certificates wrapped inside of PEM blocks.
      format = FORMAT_PEM_CERT_SEQUENCE;
      certificates.push_back(handle);
      continue;
    }

    // If the first block failed to parse as a DER certificate, and
    // formats other than PEM are acceptable, check to see if the decoded
    // data is one of the accepted formats.
    if (format & ~FORMAT_PEM_CERT_SEQUENCE) {
      for (size_t i = 0; certificates.empty() &&
           i < arraysize(kFormatDecodePriority); ++i) {
        if (format & kFormatDecodePriority[i]) {
          certificates = CreateOSCertHandlesFromBytes(decoded.c_str(),
              decoded.size(), kFormatDecodePriority[i]);
        }
      }
    }

    // Stop parsing after the first block for any format but a sequence of
    // PEM-encoded DER certificates. The case of FORMAT_PEM_CERT_SEQUENCE
    // is handled above, and continues processing until a certificate fails
    // to parse.
    break;
  }

  // Try each of the formats, in order of parse preference, to see if |data|
  // contains the binary representation of a Format, if it failed to parse
  // as a PEM certificate/chain.
  for (size_t i = 0; certificates.empty() &&
       i < arraysize(kFormatDecodePriority); ++i) {
    if (format & kFormatDecodePriority[i])
      certificates = CreateOSCertHandlesFromBytes(data, length,
                                                  kFormatDecodePriority[i]);
  }

  CertificateList results;
  // No certificates parsed.
  if (certificates.empty())
    return results;

  for (OSCertHandles::iterator it = certificates.begin();
       it != certificates.end(); ++it) {
    X509Certificate* result = CreateFromHandle(*it, SOURCE_LONE_CERT_IMPORT,
                                               OSCertHandles());
    results.push_back(scoped_refptr<X509Certificate>(result));
    FreeOSCertHandle(*it);
  }

  return results;
}

bool X509Certificate::HasExpired() const {
  return base::Time::Now() > valid_expiry();
}

bool X509Certificate::Equals(const X509Certificate* other) const {
  return IsSameOSCert(cert_handle_, other->cert_handle_);
}

bool X509Certificate::HasIntermediateCertificate(OSCertHandle cert) {
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_OPENSSL)
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

X509Certificate::X509Certificate(OSCertHandle cert_handle,
                                 Source source,
                                 const OSCertHandles& intermediates)
    : cert_handle_(DupOSCertHandle(cert_handle)),
      source_(source) {
  // Copy/retain the intermediate cert handles.
  for (size_t i = 0; i < intermediates.size(); ++i)
    intermediate_ca_certs_.push_back(DupOSCertHandle(intermediates[i]));
  // Platform-specific initialization.
  Initialize();
}

X509Certificate::~X509Certificate() {
  // We might not be in the cache, but it is safe to remove ourselves anyway.
  g_x509_certificate_cache.Get().Remove(this);
  if (cert_handle_)
    FreeOSCertHandle(cert_handle_);
  for (size_t i = 0; i < intermediate_ca_certs_.size(); ++i)
    FreeOSCertHandle(intermediate_ca_certs_[i]);
}

}  // namespace net
