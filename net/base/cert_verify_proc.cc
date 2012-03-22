// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verify_proc.h"

#include "build/build_config.h"
#include "net/base/x509_certificate.h"

namespace net {

// TODO(rsleevi): Temporary refactoring - http://crbug.com/114343
class CertVerifyProcStub : public CertVerifyProc {
 public:
  CertVerifyProcStub() {}

 private:
  virtual ~CertVerifyProcStub() {}

  // CertVerifyProc implementation
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             int flags,
                             CRLSet* crl_set,
                             CertVerifyResult* verify_result) OVERRIDE {
    return cert->Verify(hostname, flags, crl_set, verify_result);
  }
};

// static
CertVerifyProc* CertVerifyProc::CreateDefault() {
  return new CertVerifyProcStub();
}

CertVerifyProc::CertVerifyProc() {}

CertVerifyProc::~CertVerifyProc() {}

int CertVerifyProc::Verify(X509Certificate* cert,
                           const std::string& hostname,
                           int flags,
                           CRLSet* crl_set,
                           CertVerifyResult* verify_result) {
  return VerifyInternal(cert, hostname, flags, crl_set, verify_result);
}

}  // namespace net
