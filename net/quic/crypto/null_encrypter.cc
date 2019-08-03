// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/quic_data_writer.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::string;

namespace net {

const size_t kHashSize = 16;  // size of uint128 serialized

QuicData* NullEncrypter::Encrypt(StringPiece associated_data,
                                 StringPiece plaintext) {
  // TODO(rch): avoid buffer copy here
  string buffer = associated_data.as_string();
  plaintext.AppendToString(&buffer);
  uint128 hash = QuicUtils::FNV1a_128_Hash(buffer.data(), buffer.length());
  QuicDataWriter writer(plaintext.length() + kHashSize);
  writer.WriteUInt128(hash);
  writer.WriteBytes(plaintext.data(), plaintext.length());
  size_t len = writer.length();
  return new QuicData(writer.take(), len, true);
}

size_t NullEncrypter::GetMaxPlaintextSize(size_t ciphertext_size) {
  return ciphertext_size - kHashSize;
}

size_t NullEncrypter::GetCiphertextSize(size_t plaintext_size) {
  return plaintext_size + kHashSize;
}

}  // namespace net
