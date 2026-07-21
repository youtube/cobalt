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
<<<<<<< HEAD
#include "net/cert/pem.h"
#include "net/cert/pki/cert_errors.h"
=======
>>>>>>> f5d2add8af (Load SSL root certificates to avoid runtime wipe crashes (#11413))
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
<<<<<<< HEAD
#include "starboard/file.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
=======
>>>>>>> f5d2add8af (Load SSL root certificates to avoid runtime wipe crashes (#11413))
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

std::vector<std::shared_ptr<const bssl::ParsedCertificate>> GetAllCertsOnDisk() {
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
  
  std::vector<std::shared_ptr<const bssl::ParsedCertificate>> certs;
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
    bssl::PEMTokenizer pem_tokenizer(cert_buffer, {kCertificateHeader});
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
    bssl::CertErrors errors;
    auto parsed = bssl::ParsedCertificate::Create(
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

<<<<<<< HEAD
std::shared_ptr<const ParsedCertificate>
TrustStoreInMemoryStarboard::TryLoadCert(
    const base::StringPiece& cert_name) const {
  auto hash = CertNameHash(cert_name.data(), cert_name.length());
  char cert_file_name[256];
  snprintf(cert_file_name, 256, "%08lx.%d", hash, 0);

  if (trusted_cert_names_on_disk_.find(cert_file_name) ==
      trusted_cert_names_on_disk_.end()) {
    // The requested certificate is not found.
    return nullptr;
  }

  char cert_buffer[kCertBufferSize];
  base::FilePath cert_path = GetCertificateDirPath().Append(cert_file_name);
  base::File cert_file(
      cert_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  // The file was in certs directory when we iterated the directory at startup,
  // opening it should not fail.
  if (!cert_file.IsValid()) {
    NOTREACHED() << "ssl/certs/" << cert_path << " failed to open.";
    return nullptr;
  }
  int cert_size = cert_file.ReadAtCurrentPos(cert_buffer, kCertBufferSize);
  PEMTokenizer pem_tokenizer(base::StringPiece(cert_buffer, cert_size),
                             {kCertificateHeader});
  pem_tokenizer.GetNext();
  std::string decoded(pem_tokenizer.data());
  DCHECK(!pem_tokenizer.GetNext());
  auto crypto_buffer = x509_util::CreateCryptoBuffer(decoded);
  CertErrors errors;
  auto parsed = ParsedCertificate::Create(
      std::move(crypto_buffer), x509_util::DefaultParseCertificateOptions(),
      &errors);
  CHECK(parsed) << errors.ToDebugString();
  return parsed;
=======
TrustStoreInMemoryStarboard::TrustStoreInMemoryStarboard() {
  for (const auto& cert : GetAllCertsOnDisk()) {
    underlying_trust_store_.AddTrustAnchor(cert);
  }
>>>>>>> f5d2add8af (Load SSL root certificates to avoid runtime wipe crashes (#11413))
}

TrustStoreInMemoryStarboard::~TrustStoreInMemoryStarboard() = default;

void TrustStoreInMemoryStarboard::SyncGetIssuersOf(
<<<<<<< HEAD
    const ParsedCertificate* cert,
    ParsedCertificateList* issuers) {
  DCHECK(issuers);
  DCHECK(issuers->empty());
  base::AutoLock scoped_lock(load_mutex_);
  // Look up the request certificate first in the trust store in memory.
=======
    const bssl::ParsedCertificate* cert,
    bssl::ParsedCertificateList* issuers) {
>>>>>>> f5d2add8af (Load SSL root certificates to avoid runtime wipe crashes (#11413))
  underlying_trust_store_.SyncGetIssuersOf(cert, issuers);
}

<<<<<<< HEAD
CertificateTrust TrustStoreInMemoryStarboard::GetTrust(
    const ParsedCertificate* cert,
    base::SupportsUserData* debug_data) {
  base::AutoLock scoped_lock(load_mutex_);
  // Loop up the request certificate first in the trust store in memory.
  CertificateTrust trust = underlying_trust_store_.GetTrust(cert, debug_data);
  if (trust.HasUnspecifiedTrust()) {
    // If the requested certificate is not found, compute certificate hash name
    // and see if the certificate is stored on disk.
    auto parsed_cert = TryLoadCert(cert->normalized_subject().AsStringView());
    if (parsed_cert.get()) {
      trust = CertificateTrust::ForTrustAnchor();
      const_cast<TrustStoreInMemoryStarboard*>(this)
          ->underlying_trust_store_.AddTrustAnchor(parsed_cert);
    }
  }
  return trust;
=======
bssl::CertificateTrust TrustStoreInMemoryStarboard::GetTrust(
    const bssl::ParsedCertificate* cert) {
  return underlying_trust_store_.GetTrust(cert);
>>>>>>> f5d2add8af (Load SSL root certificates to avoid runtime wipe crashes (#11413))
}

}  // namespace net
