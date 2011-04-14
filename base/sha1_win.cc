// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sha1.h"

#include <windows.h>
#include <wincrypt.h>

// This file is not being compiled at the moment (see bug 47218). If we keep
// sha1 inside base, we cannot depend on src/crypto.
// #include "crypto/scoped_capi_types.h"
#include "base/logging.h"

namespace base {

std::string SHA1HashString(const std::string& str) {
  ScopedHCRYPTPROV provider;
  if (!CryptAcquireContext(provider.receive(), NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
    LOG(ERROR) << "CryptAcquireContext failed: " << GetLastError();
    return std::string(SHA1_LENGTH, '\0');
  }

  {
    ScopedHCRYPTHASH hash;
    if (!CryptCreateHash(provider, CALG_SHA1, 0, 0, hash.receive())) {
      LOG(ERROR) << "CryptCreateHash failed: " << GetLastError();
      return std::string(SHA1_LENGTH, '\0');
    }

    if (!CryptHashData(hash, reinterpret_cast<CONST BYTE*>(str.data()),
                       static_cast<DWORD>(str.length()), 0)) {
      LOG(ERROR) << "CryptHashData failed: " << GetLastError();
      return std::string(SHA1_LENGTH, '\0');
    }

    DWORD hash_len = 0;
    DWORD buffer_size = sizeof hash_len;
    if (!CryptGetHashParam(hash, HP_HASHSIZE,
                           reinterpret_cast<unsigned char*>(&hash_len),
                           &buffer_size, 0)) {
      LOG(ERROR) << "CryptGetHashParam(HP_HASHSIZE) failed: " << GetLastError();
      return std::string(SHA1_LENGTH, '\0');
    }

    std::string result;
    if (!CryptGetHashParam(hash, HP_HASHVAL,
        // We need the + 1 here not because the call will write a trailing \0,
        // but so that result.length() is correctly set to |hash_len|.
        reinterpret_cast<BYTE*>(WriteInto(&result, hash_len + 1)), &hash_len,
        0))) {
      LOG(ERROR) << "CryptGetHashParam(HP_HASHVAL) failed: " << GetLastError();
      return std::string(SHA1_LENGTH, '\0');
    }

    if (hash_len != SHA1_LENGTH) {
      LOG(ERROR) << "Returned hash value is wrong length: " << hash_len
                 << " should be " << SHA1_LENGTH;
      return std::string(SHA1_LENGTH, '\0');
    }

    return result;
  }
}

}  // namespace base
