// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_framer.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  absl::string_view crypto_input(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<quic::CryptoHandshakeMessage> handshake_message(
      quic::CryptoFramer::ParseMessage(crypto_input));

  return 0;
}
