// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include "base/lazy_instance.h"
#include "base/logging.h"

// TODO(bbudge) Replace this with a proper system header file when NaCl
// provides one.
#include "native_client/src/untrusted/irt/irt.h"

namespace {

// Create a wrapper class so we can cache the NaCl random number interface.
class URandomInterface {
 public:
  URandomInterface() {
    size_t result = nacl_interface_query(NACL_IRT_RANDOM_v0_1,
                                         &interface_,
                                         sizeof(interface_));
    DCHECK_EQ(result, sizeof(interface_)) << "Can't get random interface.";
  }

  uint64 get_random_bytes() const {
    size_t nbytes;
    uint64 result;
    int error = interface_.get_random_bytes(&result,
                                            sizeof(result),
                                            &nbytes);
    DCHECK_EQ(error, 0);
    DCHECK_EQ(nbytes, sizeof(result));
    return result;
  }

 private:
  struct nacl_irt_random interface_;
};

base::LazyInstance<URandomInterface> g_urandom_interface =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace base {

uint64 RandUint64() {
  return g_urandom_interface.Pointer()->get_random_bytes();
}

}  // namespace base

