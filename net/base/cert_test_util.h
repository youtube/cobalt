// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_TEST_UTIL_H_
#define NET_BASE_CERT_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/x509_certificate.h"

class FilePath;

namespace net {

// Returns a FilePath object representing the src/net/data/ssl/certificates
// directory in the source tree.
FilePath GetTestCertsDirectory();

CertificateList CreateCertificateListFromFile(const FilePath& certs_dir,
                                              const std::string& cert_file,
                                              int format);

// Imports a certificate file in the src/net/data/ssl/certificates directory.
// certs_dir represents the test certificates directory.  cert_file is the
// name of the certificate file. If cert_file contains multiple certificates,
// the first certificate found will be returned.
scoped_refptr<X509Certificate> ImportCertFromFile(const FilePath& certs_dir,
                                                  const std::string& cert_file);

}  // namespace net

#endif  // NET_BASE_CERT_TEST_UTIL_H_
