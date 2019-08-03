// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_FRAMER_H_
#define NET_QUIC_CRYPTO_CRYPTO_FRAMER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"

namespace net {

class CryptoFramer;
class QuicDataReader;
class QuicData;

class NET_EXPORT_PRIVATE CryptoFramerVisitorInterface {
 public:
  virtual ~CryptoFramerVisitorInterface() {}

  // Called if an error is detected.
  virtual void OnError(CryptoFramer* framer) = 0;

  // Called when a complete handshake message has been parsed.
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) = 0;
};

// A class for framing the crypto message that are exchanged in a QUIC session.
class NET_EXPORT_PRIVATE CryptoFramer {
 public:
  CryptoFramer();

  virtual ~CryptoFramer();

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will crash.  It is acceptable for the visitor to do
  // nothing.  If this is called multiple times, only the last visitor
  // will be used.  |visitor| will be owned by the framer.
  void set_visitor(CryptoFramerVisitorInterface* visitor) {
    visitor_ = visitor;
  }

  QuicErrorCode error() const {
    return error_;
  }

  // Processes input data, which must be delivered in order.  Returns
  // false if there was an error, and true otherwise.
  bool ProcessInput(base::StringPiece input);

  // Returns a new QuicData owned by the caller that contains a serialized
  // |message|, or NULL if there was an error.
  QuicData* ConstructHandshakeMessage(const CryptoHandshakeMessage& message);

 private:
  // Clears per-message state.  Does not clear the visitor.
  void Clear();

  void set_error(QuicErrorCode error) {
    error_ = error;
  }

  // Represents the current state of the framing state machine.
  enum CryptoFramerState {
    STATE_READING_TAG,
    STATE_READING_NUM_ENTRIES,
    STATE_READING_KEY_TAGS,
    STATE_READING_LENGTHS,
    STATE_READING_VALUES
  };

  // Visitor to invoke when messages are parsed.
  CryptoFramerVisitorInterface* visitor_;
  // Last error.
  QuicErrorCode error_;
  // Remaining unparsed data.
  std::string buffer_;
  // Current state of the parsing.
  CryptoFramerState state_;
  // Tag of the message currently being parsed.
  CryptoTag message_tag_;
  // Number of entires in the message currently being parsed.
  uint16 num_entries_;
  // Vector of tags in the message currently being parsed.
  CryptoTagVector tags_;
  // Length of the data associated with each tag in the message currently
  // being parsed.
  std::map<CryptoTag, size_t> tag_length_map_;
  // Data associated with each tag in the message currently being parsed.
  CryptoTagValueMap tag_value_map_;
  // Cumulative length of all values in the message currently being parsed.
  size_t values_len_;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_FRAMER_H_
