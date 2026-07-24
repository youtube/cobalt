// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/cert/internal/trust_store_in_memory_starboard.h"

#include <dirent.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "net/cert/pem.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace net {

namespace {
// Each certificate file name is 8 bit hash + ".0" suffix.
const short kCertFileNameLength = 10;
const char kCertificateHeader[] = "CERTIFICATE";
const char kSSLDirName[] = "ssl";
const char kCertsDirName[] = "certs";

base::FilePath GetCertificateDirPath() {
  base::FilePath cert_path;
  base::PathService::Get(base::DIR_EXE, &cert_path);
  cert_path = cert_path.Append(kSSLDirName).Append(kCertsDirName);
  return cert_path;
}

std::vector<std::shared_ptr<const ParsedCertificate>> GetAllCertsOnDisk() {
  DIR* sb_certs_directory = opendir(GetCertificateDirPath().value().c_str());
  if (!sb_certs_directory) {
// Unit tests, for example, do not use production certificates.
#if defined(OFFICIAL_BUILD)
    SB_CHECK(false);
#else
    DLOG(WARNING) << "ssl/certs directory is not valid, no root certificates"
                     " will be loaded";
#endif
    return {};
  }
  
  std::vector<std::shared_ptr<const ParsedCertificate>> certs;
  std::vector<char> dir_entry(kSbFileMaxName);

  struct dirent dirent_buffer;
  struct dirent* dirent;

  while (true) {
    if (dir_entry.size() < static_cast<size_t>(kSbFileMaxName) ||
        !sb_certs_directory || !dir_entry.data()) {
      break;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int result = readdir_r(sb_certs_directory, &dirent_buffer, &dirent);
#pragma GCC diagnostic pop
    if (result || !dirent) {
      break;
    }
    starboard::strlcpy(dir_entry.data(), dirent->d_name, dir_entry.size());
    if (strlen(dir_entry.data()) != kCertFileNameLength) {
      continue;
    }
    
    base::FilePath cert_path = GetCertificateDirPath().Append(dir_entry.data());
    std::string cert_buffer;
    if (!base::ReadFileToString(cert_path, &cert_buffer)) {
      DLOG(ERROR) << "Failed to read cert file: " << cert_path;
      continue;
    }
    PEMTokenizer pem_tokenizer(cert_buffer, {kCertificateHeader});
    if (!pem_tokenizer.GetNext()) {
      DLOG(ERROR) << "Failed to parse PEM from cert file: " << cert_path;
      continue;
    }
    std::string decoded(pem_tokenizer.data());
    auto crypto_buffer = x509_util::CreateCryptoBuffer(decoded);
    if (!crypto_buffer) {
      DLOG(ERROR) << "Failed to create crypto buffer for " << cert_path;
      continue;
    }
    CertErrors errors;
    auto parsed = ParsedCertificate::Create(
        std::move(crypto_buffer), x509_util::DefaultParseCertificateOptions(),
        &errors);
    if (!parsed) {
      LOG(ERROR) << "Failed to parse certificate " << cert_path << ": "
                 << errors.ToDebugString();
    } else {
      certs.push_back(parsed);
    }
  }
  closedir(sb_certs_directory);
  return certs;
}
}  // namespace

TrustStoreInMemoryStarboard::TrustStoreInMemoryStarboard() {
  for (const auto& cert : GetAllCertsOnDisk()) {
    underlying_trust_store_.AddTrustAnchor(cert);
  }
}

TrustStoreInMemoryStarboard::~TrustStoreInMemoryStarboard() = default;

void TrustStoreInMemoryStarboard::SyncGetIssuersOf(
    const ParsedCertificate* cert,
    ParsedCertificateList* issuers) {
  underlying_trust_store_.SyncGetIssuersOf(cert, issuers);
}

CertificateTrust TrustStoreInMemoryStarboard::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) {
  return underlying_trust_store_.GetTrust(cert, debug_data);
}

}  // namespace net
