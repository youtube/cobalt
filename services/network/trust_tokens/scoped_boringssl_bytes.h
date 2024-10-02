// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_SCOPED_BORINGSSL_BYTES_H_
#define SERVICES_NETWORK_TRUST_TOKENS_SCOPED_BORINGSSL_BYTES_H_

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace network {

// ScopedBoringsslBytes is a convenience helper to simplify the repeated pattern
// of feeding the BoringSSL methods a uint8_t** and later being responsible for
// |OPENSSL_free|ing the returned data.
//
// Usage: Suppose we're given a method
//   // Sets |out_data| to a pointer to a newly allocated buffer. The caller is
//   // responsible for freeing the buffer with OPENSSL_free after use.
//   void MyMethod(uint8_t** out_data, size_t* out_data_len);
//
// Call the method like this:
//   ScopedBoringsslBytes data;
//   MyMethod(data.mutable_ptr(), data.mutable_len());
//
// and |data| will magically free its own state when it leaves scope.
//
// TODO(csharrison, davidvc): Canvass other BoringSSL consumers to see if this
// would be more widely useful.
class ScopedBoringsslBytes {
 public:
  ScopedBoringsslBytes() = default;
  ~ScopedBoringsslBytes() { OPENSSL_free(ptr_); }

  bool is_valid() { return ptr_; }
  uint8_t** mutable_ptr() { return &ptr_; }
  size_t* mutable_len() { return &len_; }

  base::span<const uint8_t> as_span() const {
    CHECK(ptr_);
    return base::make_span(ptr_, len_);
  }

 private:
  size_t len_ = 0;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #constexpr-ctor-field-initializer, #addr-of
  RAW_PTR_EXCLUSION uint8_t* ptr_ = nullptr;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_SCOPED_BORINGSSL_BYTES_H_
