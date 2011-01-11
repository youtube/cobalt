// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_cert_request_info.h"

#include "net/base/x509_certificate.h"

namespace net {

SSLCertRequestInfo::SSLCertRequestInfo() {
}

void SSLCertRequestInfo::Reset() {
  host_and_port.clear();
  client_certs.clear();
}

SSLCertRequestInfo::~SSLCertRequestInfo() {
}

}  // namespace net
