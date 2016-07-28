// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_root_certs.h"

#include "base/location.h"
#include "base/logging.h"
#include "net/android/network_library.h"
#include "net/base/x509_certificate.h"

namespace net {

bool TestRootCerts::Add(X509Certificate* certificate) {
  std::string cert_bytes;
  if (!X509Certificate::GetDEREncoded(certificate->os_cert_handle(),
                                      &cert_bytes))
      return false;
  android::AddTestRootCertificate(
      reinterpret_cast<const uint8*>(cert_bytes.data()), cert_bytes.size());
  return true;
}

void TestRootCerts::Clear() {
  if (empty_)
    return;

  android::ClearTestRootCertificates();
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
