// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/metrics_hashes.h"

#include <string.h>

#include "base/check_op.h"
#include "base/hash/md5.h"
#include "base/sys_byteorder.h"

namespace base {
namespace {

// Converts the 8-byte prefix of an MD5 hash into a uint64_t value.
inline uint64_t DigestToUInt64(const base::MD5Digest& digest) {
  uint64_t value;
  DCHECK_GE(sizeof(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  return base::NetToHost64(value);
}

// Converts the 4-byte prefix of an MD5 hash into a uint32_t value.
inline uint32_t DigestToUInt32(const base::MD5Digest& digest) {
  uint32_t value;
  DCHECK_GE(sizeof(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  return base::NetToHost32(value);
}

}  // namespace

uint64_t HashMetricName(base::StringPiece name) {
  // Corresponding Python code for quick look up:
  //
  //   import struct
  //   import hashlib
  //   struct.unpack('>Q', hashlib.md5(name.encode('utf-8')).digest()[:8])[0]
  //
  base::MD5Digest digest;
  base::MD5Sum(name.data(), name.size(), &digest);
  return DigestToUInt64(digest);
}

uint32_t HashMetricNameAs32Bits(base::StringPiece name) {
  base::MD5Digest digest;
  base::MD5Sum(name.data(), name.size(), &digest);
  return DigestToUInt32(digest);
}

}  // namespace base
