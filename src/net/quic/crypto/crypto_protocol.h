// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
#define NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"

namespace net {

typedef uint32 CryptoTag;
typedef std::map<CryptoTag, base::StringPiece> CryptoTagValueMap;
typedef std::vector<CryptoTag> CryptoTagVector;
struct NET_EXPORT_PRIVATE CryptoHandshakeMessage {
  CryptoHandshakeMessage();
  ~CryptoHandshakeMessage();
  CryptoTag tag;
  CryptoTagValueMap tag_value_map;
};

// Crypto tags are written to the wire with a big-endian
// representation of the name of the tag.  For example
// the client hello tag (CHLO) will be written as the
// following 4 bytes: 'C' 'H' 'L' 'O'.  Since it is
// stored in memory as a little endian uint32, we need
// to reverse the order of the bytes.
#define MAKE_TAG(a, b, c, d) (d << 24) + (c << 16) + (b << 8) + a

const CryptoTag kCHLO = MAKE_TAG('C', 'H', 'L', 'O');  // Client hello
const CryptoTag kSHLO = MAKE_TAG('S', 'H', 'L', 'O');  // Server hello

// AEAD algorithms
const CryptoTag kNULL = MAKE_TAG('N', 'U', 'L', 'L');  // null algorithm
const CryptoTag kAESH = MAKE_TAG('A', 'E', 'S', 'H');  // AES128 + SHA256

const size_t kMaxEntries = 16;  // Max number of entries in a message.

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
