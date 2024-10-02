// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/ukey2/src/src/main/cpp/include/securegcm/ukey2_handshake.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"

const securegcm::UKey2Handshake::HandshakeCipher kCipher =
    securegcm::UKey2Handshake::HandshakeCipher::P256_SHA512;

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOG_FATAL);

    // Create instance once to be reused between fuzzing rounds.
    server = securegcm::UKey2Handshake::ForResponder(kCipher);
    CHECK(server);
  }

  std::unique_ptr<securegcm::UKey2Handshake> server;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::NoDestructor<Environment> environment;

  std::string buffer(data, data + size);
  environment->server->ParseHandshakeMessage(buffer);

  return 0;
}
