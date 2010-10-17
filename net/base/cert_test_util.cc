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
X509Certificate* LoadTemporaryRootCert(const FilePath& filename) {
  OpenSSLInitSingleton* openssl_init = GetOpenSSLInitSingleton();

  std::string rawcert;
  if (!file_util::ReadFileToString(filename, &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.value();
    return NULL;
  }

  X509Certificate* certificate = X509Certificate::CreateFromBytes(
      rawcert.c_str(), rawcert.length());

  X509* x509_cert =
      X509Certificate::DupOSCertHandle(certificate->os_cert_handle());
  if (!X509_STORE_add_cert(openssl_init->x509_store(), x509_cert)) {
    LOG(ERROR) << "X509_STORE_add_cert error: " << ERR_get_error();
    X509_free(x509_cert);
    return NULL;
  }

  return certificate;
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
