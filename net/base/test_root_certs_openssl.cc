// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_root_certs.h"

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "base/logging.h"
#include "base/tracked.h"
#include "crypto/openssl_util.h"
#include "net/base/x509_certificate.h"

namespace net {

bool TestRootCerts::Add(X509Certificate* certificate) {
  if (!X509_STORE_add_cert(X509Certificate::cert_store(),
                           certificate->os_cert_handle())) {
    unsigned long error_code = ERR_peek_error();
    if (ERR_GET_LIB(error_code) != ERR_LIB_X509 ||
        ERR_GET_REASON(error_code) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
      crypto::ClearOpenSSLERRStack(FROM_HERE);
      return false;
    }
    ERR_clear_error();
  }

  empty_ = false;
  return true;
}

void TestRootCerts::Clear() {
  if (empty_)
    return;

  X509Certificate::ResetCertStore();
  empty_ = true;
}

bool TestRootCerts::IsEmpty() const {
  return empty_;
}

TestRootCerts::~TestRootCerts() {}

void TestRootCerts::Init() {
  empty_ = true;
}

}  // namespace net
