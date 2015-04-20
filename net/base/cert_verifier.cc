// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verifier.h"

#include "net/base/cert_verify_proc.h"
#include "net/base/multi_threaded_cert_verifier.h"

namespace net {

CertVerifier* CertVerifier::CreateDefault() {
  return new MultiThreadedCertVerifier(CertVerifyProc::CreateDefault());
}

}  // namespace net
