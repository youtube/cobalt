// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/null_decrypter.h"

namespace net {

// static
QuicDecrypter* QuicDecrypter::Create(CryptoTag algorithm) {
  switch (algorithm) {
    case kNULL:
      return new NullDecrypter();
    default:
      LOG(FATAL) << "Unsupported algorithm: " << algorithm;
      return NULL;
  }
}

}  // namespace net
