// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sha1.h"

#include <windows.h>
#include <wincrypt.h>

#include "base/logging.h"
#include "base/string_util.h"

namespace base {

// Implementation of SHA-1 using Windows CryptoAPI.

// Usage example:
//
// SecureHashAlgorithm sha;
// while(there is data to hash)
//   sha.Update(moredata, size of data);
// sha.Final();
// memcpy(somewhere, sha.Digest(), 20);
//
// to reuse the instance of sha, call sha.Init();

class SecureHashAlgorithm {
 public:
  SecureHashAlgorithm() : prov_(NULL), hash_(NULL) { Init(); }

  void Init();
  void Update(const void* data, size_t nbytes);
  void Final();

  // 20 bytes of message digest.
  const unsigned char* Digest() const {
    return reinterpret_cast<const unsigned char*>(result_.data());
  }

 private:
  // Cleans up prov_, hash_, and result_.
  void Cleanup();

  HCRYPTPROV prov_;
  HCRYPTHASH hash_;
  std::string result_;
};

void SecureHashAlgorithm::Init() {
  Cleanup();

  if (!CryptAcquireContext(&prov_, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    LOG(ERROR) << "CryptAcquireContext failed: " << GetLastError();
    return;
  }

  // Initialize the hash.
  if (!CryptCreateHash(prov_, CALG_SHA1, 0, 0, &hash_)) {
    LOG(ERROR) << "CryptCreateHash failed: " << GetLastError();
    return;
  }
}

void SecureHashAlgorithm::Update(const void* data, size_t nbytes) {
  BOOL ok = CryptHashData(hash_, reinterpret_cast<CONST BYTE*>(data),
                          static_cast<DWORD>(nbytes), 0);
  CHECK(ok) << "CryptHashData failed: " << GetLastError();
}

void SecureHashAlgorithm::Final() {
  DWORD hash_len = 0;
  DWORD buffer_size = sizeof(hash_len);
  if (!CryptGetHashParam(hash_, HP_HASHSIZE,
                         reinterpret_cast<unsigned char*>(&hash_len),
                         &buffer_size, 0)) {
    LOG(ERROR) << "CryptGetHashParam(HP_HASHSIZE) failed: " << GetLastError();
    result_.assign(SHA1_LENGTH, '\0');
    return;
  }

  // Get the hash data.
  if (!CryptGetHashParam(hash_, HP_HASHVAL,
                         reinterpret_cast<BYTE*>(WriteInto(&result_,
                                                           hash_len + 1)),
                         &hash_len, 0)) {
    LOG(ERROR) << "CryptGetHashParam(HP_HASHVAL) failed: " << GetLastError();
    result_.assign(SHA1_LENGTH, '\0');
    return;
  }
}

void SecureHashAlgorithm::Cleanup() {
  BOOL ok;
  if (hash_) {
    ok = CryptDestroyHash(hash_);
    DCHECK(ok);
    hash_ = NULL;
  }
  if (prov_) {
    ok = CryptReleaseContext(prov_, 0);
    DCHECK(ok);
    prov_ = NULL;
  }
  result_.clear();
}

std::string SHA1HashString(const std::string& str) {
  SecureHashAlgorithm sha;
  sha.Update(str.c_str(), str.length());
  sha.Final();
  std::string out(reinterpret_cast<const char*>(sha.Digest()), SHA1_LENGTH);
  return out;
}

}  // namespace base
