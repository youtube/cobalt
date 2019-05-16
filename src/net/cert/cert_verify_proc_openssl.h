// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFY_PROC_OPENSSL_H_
#define NET_BASE_CERT_VERIFY_PROC_OPENSSL_H_

#include "net/cert/cert_verify_proc.h"

// Note: CertVerifyProcOpenSSL was copied from the old Chromium(m27) to support
// verifying certs with openSSL which has been used by Cobalt since day 1.
// This Method has somehow been deprecated by new Chromium(m70).
// TODO[johnx]: Switch to Chromium's new CertVerifyProcBuiltin soon after the
//              rebase.

namespace net {

// Performs certificate path construction and validation using OpenSSL.
class CertVerifyProcOpenSSL : public CertVerifyProc {
 public:
  CertVerifyProcOpenSSL();

 protected:
  virtual ~CertVerifyProcOpenSSL();

 private:
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             const std::string& ocsp_response,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result) override;
  virtual bool SupportsAdditionalTrustAnchors() const override { return false; }
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFY_PROC_OPENSSL_H_
