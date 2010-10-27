// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_test_util.h"

#include "build/build_config.h"

#if defined(USE_OPENSSL)
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "net/base/openssl_util.h"
#elif defined(USE_NSS)
#include <cert.h>
#include "base/nss_util.h"
#elif defined(OS_MACOSX)
#include <Security/Security.h>
#include "base/mac/scoped_cftyperef.h"
#endif

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "net/base/x509_certificate.h"

namespace net {

#if defined(USE_OPENSSL)
X509Certificate* AddTemporaryRootCertToStore(X509* x509_cert) {
  OpenSSLInitSingleton* openssl_init = GetOpenSSLInitSingleton();

  if (!X509_STORE_add_cert(openssl_init->x509_store(), x509_cert)) {
    unsigned long error_code = ERR_get_error();
    if (ERR_GET_LIB(error_code) != ERR_LIB_X509 ||
        ERR_GET_REASON(error_code) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
      do {
        LOG(ERROR) << "X509_STORE_add_cert error: " << error_code;
      } while ((error_code = ERR_get_error()) != 0);
      return NULL;
    }
  }
  return X509Certificate::CreateFromHandle(
      x509_cert, X509Certificate::SOURCE_LONE_CERT_IMPORT,
      X509Certificate::OSCertHandles());
}

X509Certificate* LoadTemporaryRootCert(const FilePath& filename) {
  EnsureOpenSSLInit();

  std::string rawcert;
  if (!file_util::ReadFileToString(filename, &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return NULL;
  }

  ScopedSSL<BIO, BIO_free_all> cert_bio(
      BIO_new_mem_buf(const_cast<char*>(rawcert.c_str()),
                      rawcert.length()));
  if (!cert_bio.get()) {
    LOG(ERROR) << "Can't create read-only BIO " << filename.value();
    return NULL;
  }

  ScopedSSL<X509, X509_free> pem_cert(PEM_read_bio_X509(cert_bio.get(),
                                                        NULL, NULL, NULL));
  if (pem_cert.get())
    return AddTemporaryRootCertToStore(pem_cert.get());

  // File does not contain PEM data, let's try DER.
  const unsigned char* der_data =
      reinterpret_cast<const unsigned char*>(rawcert.c_str());
  int der_length = rawcert.length();
  ScopedSSL<X509, X509_free> der_cert(d2i_X509(NULL, &der_data, der_length));
  if (der_cert.get())
    return AddTemporaryRootCertToStore(der_cert.get());

  LOG(ERROR) << "Can't parse certificate " << filename.value();
  return NULL;
}
#elif defined(USE_NSS)
X509Certificate* LoadTemporaryRootCert(const FilePath& filename) {
  base::EnsureNSSInit();

  std::string rawcert;
  if (!file_util::ReadFileToString(filename, &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return NULL;
  }

  CERTCertificate *cert;
  cert = CERT_DecodeCertFromPackage(const_cast<char *>(rawcert.c_str()),
                                    rawcert.length());
  if (!cert) {
    LOG(ERROR) << "Can't convert certificate " << filename.value();
    return NULL;
  }

  // TODO(port): remove this const_cast after NSS 3.12.3 is released
  CERTCertTrust trust;
  int rv = CERT_DecodeTrustString(&trust, const_cast<char *>("TCu,Cu,Tu"));
  if (rv != SECSuccess) {
    LOG(ERROR) << "Can't decode trust string";
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  rv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, &trust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Can't change trust for certificate " << filename.value();
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  X509Certificate* result = X509Certificate::CreateFromHandle(
      cert,
      X509Certificate::SOURCE_LONE_CERT_IMPORT,
      X509Certificate::OSCertHandles());
  CERT_DestroyCertificate(cert);
  return result;
}
#endif

#if defined(OS_MACOSX)
X509Certificate* LoadTemporaryRootCert(const FilePath& filename) {
  std::string rawcert;
  if (!file_util::ReadFileToString(filename, &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return NULL;
  }

  CFDataRef pem = CFDataCreate(kCFAllocatorDefault,
                               reinterpret_cast<const UInt8*>(rawcert.data()),
                               static_cast<CFIndex>(rawcert.size()));
  if (!pem)
    return NULL;
  base::mac::ScopedCFTypeRef<CFDataRef> scoped_pem(pem);

  SecExternalFormat input_format = kSecFormatUnknown;
  SecExternalItemType item_type = kSecItemTypeUnknown;
  CFArrayRef cert_array = NULL;
  if (SecKeychainItemImport(pem, NULL, &input_format, &item_type, 0, NULL, NULL,
                            &cert_array))
    return NULL;
  base::mac::ScopedCFTypeRef<CFArrayRef> scoped_cert_array(cert_array);

  if (!CFArrayGetCount(cert_array))
    return NULL;

  SecCertificateRef cert_ref = static_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(cert_array, 0)));

  return X509Certificate::CreateFromHandle(cert_ref,
      X509Certificate::SOURCE_LONE_CERT_IMPORT,
      X509Certificate::OSCertHandles());
}
#endif

}  // namespace net
