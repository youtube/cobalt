// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"
#include "net/base/net_export.h"

namespace net {

__attribute__((visibility("default"))) bool TestRootCerts::AddImpl(net::X509Certificate* certificate) {
  return true;
}

void TestRootCerts::ClearImpl() {}

TestRootCerts::~TestRootCerts() = default;

void TestRootCerts::Init() {}

}  // namespace net
