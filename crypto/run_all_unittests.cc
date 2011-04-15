// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "crypto/nss_util.h"

int main(int argc, char** argv) {
#if defined(USE_NSS)
  // This is most likely not needed, but it basically replaces a similar call
  // that was performed on test_support_base.
  // TODO(rvargas) Bug 79359: remove this.
  crypto::EnsureNSSInit();
#endif  // defined(USE_NSS)

  return base::TestSuite(argc, argv).Run();
}
