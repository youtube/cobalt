// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_TEST_UTIL_H_
#define NET_BASE_CERT_TEST_UTIL_H_

#include "base/file_path.h"
#include "build/build_config.h"

namespace net {

class X509Certificate;

#if defined(USE_NSS) || defined(OS_MACOSX)
// Loads and trusts a root CA certificate (stored in a file) temporarily.
// TODO(wtc): Implement this function on Windows (http://crbug.com/8470).
X509Certificate* LoadTemporaryRootCert(const FilePath& filename);
#endif

}  // namespace net

#endif  // NET_BASE_CERT_TEST_UTIL_H_
