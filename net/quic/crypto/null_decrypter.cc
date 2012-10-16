// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_decrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/quic_data_reader.h"

using base::StringPiece;
using std::string;

namespace net {

QuicData* NullDecrypter::Decrypt(StringPiece associated_data,
                                 StringPiece ciphertext) {
  QuicDataReader reader(ciphertext.data(), ciphertext.length());

  uint128 hash;
  if (!reader.ReadUInt128(&hash)) {
    return NULL;
  }

  StringPiece plaintext = reader.ReadRemainingPayload();

  // TODO(rch): avoid buffer copy here
  string buffer = associated_data.as_string();
  plaintext.AppendToString(&buffer);

  if (hash != QuicUtils::FNV1a_128_Hash(buffer.data(), buffer.length())) {
    return NULL;
  }
  return new QuicData(plaintext.data(), plaintext.length());
}

}  // namespace net
