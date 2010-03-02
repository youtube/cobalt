// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

CertDatabase::CertDatabase() {
  NOTIMPLEMENTED();
}

int CertDatabase::CheckUserCert(X509Certificate* cert) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int CertDatabase::AddUserCert(X509Certificate* cert) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

void CertDatabase::Init() {
  NOTIMPLEMENTED();
}

}  // namespace net
