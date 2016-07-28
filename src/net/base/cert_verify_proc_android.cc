// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verify_proc_android.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "net/android/network_library.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

namespace {

// Returns true if the certificate verification call was successful (regardless
// of its result), i.e. if |verify_result| was set. Otherwise returns false.
bool VerifyFromAndroidTrustManager(const std::vector<std::string>& cert_bytes,
                                   CertVerifyResult* verify_result) {
  // TODO(joth): Fetch the authentication type from SSL rather than hardcode.
  bool verified = true;
  android::VerifyResult result =
      android::VerifyX509CertChain(cert_bytes, "RSA");
  switch (result) {
    case android::VERIFY_OK:
      break;
    case android::VERIFY_NO_TRUSTED_ROOT:
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;
    case android::VERIFY_INVOCATION_ERROR:
      verified = false;
      break;
    default:
      verify_result->cert_status |= CERT_STATUS_INVALID;
      break;
  }
  return verified;
}

bool GetChainDEREncodedBytes(X509Certificate* cert,
                             std::vector<std::string>* chain_bytes) {
  X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
  X509Certificate::OSCertHandles cert_handles =
      cert->GetIntermediateCertificates();

  // Make sure the peer's own cert is the first in the chain, if it's not
  // already there.
  if (cert_handles.empty() || cert_handles[0] != cert_handle)
    cert_handles.insert(cert_handles.begin(), cert_handle);

  chain_bytes->reserve(cert_handles.size());
  for (X509Certificate::OSCertHandles::const_iterator it =
       cert_handles.begin(); it != cert_handles.end(); ++it) {
    std::string cert_bytes;
    if(!X509Certificate::GetDEREncoded(*it, &cert_bytes))
      return false;
    chain_bytes->push_back(cert_bytes);
  }
  return true;
}

}  // namespace

CertVerifyProcAndroid::CertVerifyProcAndroid() {}

CertVerifyProcAndroid::~CertVerifyProcAndroid() {}

int CertVerifyProcAndroid::VerifyInternal(X509Certificate* cert,
                                          const std::string& hostname,
                                          int flags,
                                          CRLSet* crl_set,
                                          CertVerifyResult* verify_result) {
  if (!cert->VerifyNameMatch(hostname))
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;

  std::vector<std::string> cert_bytes;
  if (!GetChainDEREncodedBytes(cert, &cert_bytes))
    return ERR_CERT_INVALID;
  if (!VerifyFromAndroidTrustManager(cert_bytes, verify_result)) {
    NOTREACHED();
    return ERR_FAILED;
  }
  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  // TODO(ppi): Implement missing functionality: yielding the constructed trust
  // chain, public key hashes of its certificates and |is_issued_by_known_root|
  // flag. All of the above require specific support from the platform, missing
  // in the Java APIs. See also: http://crbug.com/116838

  // Until the required support is available in the platform, we don't know if
  // the trust root at the end of the chain was standard or user-added, so we
  // mark all correctly verified certificates as issued by a known root.
  verify_result->is_issued_by_known_root = true;

  return OK;
}

}  // namespace net
