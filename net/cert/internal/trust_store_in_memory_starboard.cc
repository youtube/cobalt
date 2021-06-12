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

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/string.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace net {

namespace {
// PEM encoded DER cert is usually around or less than 2000 bytes long.
const short kCertBufferSize = 8 * 1024;
// Each certificate file name is 8 bit hash + ".0" suffix.
const short kCertFileNameLength = 10;
const char kCertificateHeader[] = "CERTIFICATE";
const char kSSLDirName[] = "ssl";
const char kCertsDirName[] = "certs";

// Essentially an X509_NAME_hash without using X509_NAME. We did not use the
// boringSSL function directly because net does not store certs with X509
// struct anymore and converting binary certs to X509_NAME is slightly
// expensive.
unsigned long CertNameHash(const void* data, size_t length) {
  unsigned long ret = 0;
  unsigned char md[SHA_DIGEST_LENGTH];
  if (!EVP_Digest(data, length, md, NULL, EVP_sha1(), NULL))
    return 0;

  ret = (((unsigned long)md[0]) | ((unsigned long)md[1] << 8L) |
         ((unsigned long)md[2] << 16L) | ((unsigned long)md[3] << 24L)) &
        0xffffffffL;
  return ret;
}

base::FilePath GetCertificateDirPath() {
  base::FilePath cert_path;
  base::PathService::Get(base::DIR_EXE, &cert_path);
  cert_path = cert_path.Append(kSSLDirName).Append(kCertsDirName);
  return std::move(cert_path);
}

std::unordered_set<std::string> GetCertNamesOnDisk() {
  auto sb_certs_directory =
      SbDirectoryOpen(GetCertificateDirPath().value().c_str(), nullptr);
  if (!SbDirectoryIsValid(sb_certs_directory)) {
// Unit tests, for example, do not use production certificates.
#if defined(STARBOARD_BUILD_TYPE_QA) || defined(STARBOARD_BUILD_TYPE_GOLD)
    SB_CHECK(false);
#else
    DLOG(WARNING) << "ssl/certs directory is not valid, no root certificates"
                     " will be loaded";
#endif
    return std::unordered_set<std::string>();
  }
  std::unordered_set<std::string> trusted_certs_on_disk;
#if SB_API_VERSION >= 12
  std::vector<char> dir_entry(kSbFileMaxName);

  while (SbDirectoryGetNext(sb_certs_directory, dir_entry.data(),
                            dir_entry.size())) {
    if (strlen(dir_entry.data()) != kCertFileNameLength) {
      continue;
    }
    trusted_certs_on_disk.emplace(dir_entry.data());
  }
#else   // SB_API_VERSION >= 12
  SbDirectoryEntry dir_entry;

  while (SbDirectoryGetNext(sb_certs_directory, &dir_entry)) {
    if (strlen(dir_entry.name) != kCertFileNameLength) {
      continue;
    }
    trusted_certs_on_disk.emplace(dir_entry.name);
  }
#endif  // SB_API_VERSION >= 12

  SbDirectoryClose(sb_certs_directory);
  return std::move(trusted_certs_on_disk);
}
}  // namespace

scoped_refptr<ParsedCertificate> TrustStoreInMemoryStarboard::TryLoadCert(
    const base::StringPiece& cert_name) const {
  auto hash = CertNameHash(cert_name.data(), cert_name.length());
  char cert_file_name[256];
  SbStringFormatF(cert_file_name, 256, "%08lx.%d", hash, 0);

  if (trusted_cert_names_on_disk_.find(cert_file_name) ==
      trusted_cert_names_on_disk_.end()) {
    // The requested certificate is not found.
    return nullptr;
  }

  SbFileError out_error;
  char cert_buffer[kCertBufferSize];
  base::FilePath cert_path = GetCertificateDirPath().Append(cert_file_name);
  SbFile sb_cert_file =
      SbFileOpen(cert_path.value().c_str(), kSbFileOpenOnly | kSbFileRead,
                 nullptr, &out_error);
  // The file was in certs directory when we iterated the directory at startup,
  // opening it should not fail.
  if (!SbFileIsValid(sb_cert_file)) {
    NOTREACHED() << "ssl/certs/" << cert_path << " failed to open.";
    return nullptr;
  }
  int cert_size = SbFileReadAll(sb_cert_file, cert_buffer, kCertBufferSize);
  SbFileClose(sb_cert_file);
  PEMTokenizer pem_tokenizer(base::StringPiece(cert_buffer, cert_size),
                             {kCertificateHeader});
  pem_tokenizer.GetNext();
  std::string decoded(pem_tokenizer.data());
  DCHECK(!pem_tokenizer.GetNext());
  bssl::UniquePtr<CRYPTO_BUFFER> crypto_buffer =
      X509Certificate::CreateCertBufferFromBytes(decoded.data(),
                                                 decoded.length());
  DCHECK(crypto_buffer);
  CertErrors errors;
  auto parsed = ParsedCertificate::Create(
      bssl::UpRef(crypto_buffer.get()),
      x509_util::DefaultParseCertificateOptions(), &errors);
  CHECK(parsed) << errors.ToDebugString();
  return parsed;
}

TrustStoreInMemoryStarboard::TrustStoreInMemoryStarboard()
    : trusted_cert_names_on_disk_(std::move(GetCertNamesOnDisk())) {}

TrustStoreInMemoryStarboard::~TrustStoreInMemoryStarboard() = default;

void TrustStoreInMemoryStarboard::SyncGetIssuersOf(
    const ParsedCertificate* cert,
    ParsedCertificateList* issuers) {
  DCHECK(issuers);
  DCHECK(issuers->empty());
  starboard::ScopedLock scoped_lock(load_mutex_);
  // Look up the request certificate first in the trust store in memory.
  underlying_trust_store_.SyncGetIssuersOf(cert, issuers);
  if (issuers->empty()) {
    // If the requested certificate is not found, compute certificate hash name
    // and see if the certificate is stored on disk.
    auto parsed_cert = TryLoadCert(cert->normalized_issuer().AsStringPiece());
    if (parsed_cert.get()) {
      issuers->push_back(parsed_cert);
      underlying_trust_store_.AddTrustAnchor(parsed_cert);
    }
  }
}

void TrustStoreInMemoryStarboard::GetTrust(
    const scoped_refptr<ParsedCertificate>& cert,
    CertificateTrust* trust) const {
  DCHECK(trust);
  starboard::ScopedLock scoped_lock(load_mutex_);
  // Loop up the request certificate first in the trust store in memory.
  underlying_trust_store_.GetTrust(cert, trust);
  if (trust->HasUnspecifiedTrust()) {
    // If the requested certificate is not found, compute certificate hash name
    // and see if the certificate is stored on disk.
    auto parsed_cert = TryLoadCert(cert->normalized_subject().AsStringPiece());
    if (parsed_cert.get()) {
      *trust = CertificateTrust::ForTrustAnchor();
      const_cast<TrustStoreInMemoryStarboard*>(this)
          ->underlying_trust_store_.AddTrustAnchor(parsed_cert);
    }
  }
}

}  // namespace net
