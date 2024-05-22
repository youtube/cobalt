// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/cert_issuer_source_sync_unittest.h"

namespace net {

// This suite is only instantiated when NSS is used.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    CertIssuerSourceSyncNotNormalizedTest);

}  // namespace net