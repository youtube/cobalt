// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PLATFORM_KEY_UTIL_H_
#define NET_SSL_SSL_PLATFORM_KEY_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

class X509Certificate;

// Returns a task runner to serialize all private key operations on a single
// background thread to avoid problems with buggy smartcards. Its underlying
// Thread is non-joinable and as such provides
// TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN semantics.
NET_EXPORT_PRIVATE scoped_refptr<base::SingleThreadTaskRunner>
GetSSLPlatformKeyTaskRunner();

// Returns the public key of |certificate| as an |EVP_PKEY| or nullptr on error.
bssl::UniquePtr<EVP_PKEY> GetClientCertPublicKey(
    const X509Certificate* certificate);

// Determines the key type and maximum signature length of |certificate|'s
// public key. |*out_type| will be set to one of the |EVP_PKEY_*| values from
// BoringSSL.
NET_EXPORT_PRIVATE bool GetClientCertInfo(const X509Certificate* certificate,
                                          int* out_type,
                                          size_t* out_max_length);

// Returns the encoded form of |digest| for use with RSA-PSS with |pubkey|,
// using |md| as the hash function and MGF-1 function, and the digest size of
// |md| as the salt length.
absl::optional<std::vector<uint8_t>> AddPSSPadding(
    EVP_PKEY* pubkey,
    const EVP_MD* md,
    base::span<const uint8_t> digest);

}  // namespace net

#endif  // NET_SSL_SSL_PLATFORM_KEY_UTIL_H_
