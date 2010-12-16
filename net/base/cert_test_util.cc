// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_test_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "net/base/x509_certificate.h"

namespace net {

FilePath GetTestCertsDirectory() {
  FilePath certs_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
  certs_dir = certs_dir.AppendASCII("net");
  certs_dir = certs_dir.AppendASCII("data");
  certs_dir = certs_dir.AppendASCII("ssl");
  certs_dir = certs_dir.AppendASCII("certificates");
  return certs_dir;
}

scoped_refptr<X509Certificate> ImportCertFromFile(
    const FilePath& certs_dir,
    const std::string& cert_file) {
  FilePath cert_path = certs_dir.AppendASCII(cert_file);
  std::string cert_data;
  if (!file_util::ReadFileToString(cert_path, &cert_data))
    return NULL;

  CertificateList certs_in_file =
      X509Certificate::CreateCertificateListFromBytes(
          cert_data.data(), cert_data.size(), X509Certificate::FORMAT_AUTO);
  if (certs_in_file.empty())
    return NULL;
  return certs_in_file[0];
}

}  // namespace net
