// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_test_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "net/base/ev_root_ca_metadata.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

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

CertificateList CreateCertificateListFromFile(
    const FilePath& certs_dir,
    const std::string& cert_file,
    int format) {
  FilePath cert_path = certs_dir.AppendASCII(cert_file);
  std::string cert_data;
  if (!file_util::ReadFileToString(cert_path, &cert_data))
    return CertificateList();
  return X509Certificate::CreateCertificateListFromBytes(cert_data.data(),
                                                         cert_data.size(),
                                                         format);
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

ScopedTestEVPolicy::ScopedTestEVPolicy(EVRootCAMetadata* ev_root_ca_metadata,
                                       const SHA1HashValue& fingerprint,
                                       const char* policy)
    : fingerprint_(fingerprint),
      ev_root_ca_metadata_(ev_root_ca_metadata) {
  EXPECT_TRUE(ev_root_ca_metadata->AddEVCA(fingerprint, policy));
}

ScopedTestEVPolicy::~ScopedTestEVPolicy() {
  EXPECT_TRUE(ev_root_ca_metadata_->RemoveEVCA(fingerprint_));
}

}  // namespace net
