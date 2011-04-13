// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_info.h"

#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"

namespace net {

SSLInfo::SSLInfo()
    : cert_status(0),
      security_bits(-1),
      connection_status(0),
      is_issued_by_known_root(false) {
}

SSLInfo::SSLInfo(const SSLInfo& info)
    : cert(info.cert),
      cert_status(info.cert_status),
      security_bits(info.security_bits),
      connection_status(info.connection_status),
      is_issued_by_known_root(info.is_issued_by_known_root),
      public_key_hashes(info.public_key_hashes) {
}

SSLInfo::~SSLInfo() {
}

SSLInfo& SSLInfo::operator=(const SSLInfo& info) {
  cert = info.cert;
  cert_status = info.cert_status;
  security_bits = info.security_bits;
  connection_status = info.connection_status;
  public_key_hashes = info.public_key_hashes;
  is_issued_by_known_root = info.is_issued_by_known_root;
  return *this;
}

void SSLInfo::Reset() {
  cert = NULL;
  cert_status = 0;
  security_bits = -1;
  connection_status = 0;
  is_issued_by_known_root = false;
  public_key_hashes.clear();
}

void SSLInfo::SetCertError(int error) {
  cert_status |= MapNetErrorToCertStatus(error);
}

}  // namespace net
