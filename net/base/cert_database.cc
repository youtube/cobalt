// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include "net/base/x509_certificate.h"

namespace net {

CertDatabase::ImportCertFailure::ImportCertFailure(
    X509Certificate* cert, int err)
    : certificate(cert), net_error(err) {
}

CertDatabase::ImportCertFailure::~ImportCertFailure() {
}

}  // namespace net
