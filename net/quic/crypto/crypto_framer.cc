// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_framer.h"

#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_data_writer.h"
#include "net/quic/quic_protocol.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kCryptoTagSize = sizeof(uint32);
const size_t kNumEntriesSize = sizeof(uint16);
const size_t kValueLenSize = sizeof(uint16);

}  // namespace

CryptoFramer::CryptoFramer()
    : visitor_(NULL),
      message_tag_(0),
      num_entries_(0),
      values_len_(0) {
  Clear();
}

CryptoFramer::~CryptoFramer() {}

bool CryptoFramer::ProcessInput(StringPiece input) {
  DCHECK_EQ(QUIC_NO_ERROR, error_);
  if (error_ != QUIC_NO_ERROR) {
    return false;
  }
  // Add this data to the buffer.
  buffer_.append(input.data(), input.length());
  QuicDataReader reader(buffer_.data(), buffer_.length());

  switch (state_) {
    case STATE_READING_TAG:
      if (reader.BytesRemaining() < kCryptoTagSize) {
        break;
      }
      reader.ReadUInt32(&message_tag_);
      state_ = STATE_READING_NUM_ENTRIES;
    case STATE_READING_NUM_ENTRIES:
      if (reader.BytesRemaining() < kNumEntriesSize) {
        break;
      }
      reader.ReadUInt16(&num_entries_);
      if (num_entries_ > kMaxEntries) {
        error_ = QUIC_CRYPTO_TOO_MANY_ENTRIES;
        return false;
      }
      state_ = STATE_READING_KEY_TAGS;
    case STATE_READING_KEY_TAGS:
      if (reader.BytesRemaining() < num_entries_ * kCryptoTagSize) {
        break;
      }
      for (int i = 0; i < num_entries_; ++i) {
        CryptoTag tag;
        reader.ReadUInt32(&tag);
        if (i > 0 && tag <= tags_.back()) {
          error_ = QUIC_CRYPTO_TAGS_OUT_OF_ORDER;
          return false;
        }
        tags_.push_back(tag);
      }
      state_ = STATE_READING_LENGTHS;
    case STATE_READING_LENGTHS:
      if (reader.BytesRemaining() < num_entries_ * kValueLenSize) {
        break;
      }
      values_len_ = 0;
      for (int i = 0; i < num_entries_; ++i) {
        uint16 len;
        reader.ReadUInt16(&len);
        tag_length_map_[tags_[i]] = len;
        values_len_ += len;
        if (len == 0 && i != num_entries_ - 1) {
          error_ = QUIC_CRYPTO_INVALID_VALUE_LENGTH;
          return false;
        }
      }
      state_ = STATE_READING_VALUES;
    case STATE_READING_VALUES:
      if (reader.BytesRemaining() < values_len_) {
        break;
      }
      for (int i = 0; i < num_entries_; ++i) {
        StringPiece value;
        reader.ReadStringPiece(&value, tag_length_map_[tags_[i]]);
        tag_value_map_[tags_[i]] = value;
      }
      CryptoHandshakeMessage message;
      message.tag = message_tag_;
      message.tag_value_map.swap(tag_value_map_);
      visitor_->OnHandshakeMessage(message);
      Clear();
      state_ = STATE_READING_TAG;
      break;
  }
  // Save any remaining data.
  buffer_ = reader.PeekRemainingPayload().as_string();
  return true;
}

QuicData* CryptoFramer::ConstructHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  if (message.tag_value_map.size() > kMaxEntries) {
    return NULL;
  }
  size_t len = sizeof(uint32);  // message tag
  len += sizeof(uint16);  // number of map entries
  CryptoTagValueMap::const_iterator it = message.tag_value_map.begin();
  while (it != message.tag_value_map.end()) {
    len += sizeof(uint32);  // tag
    len += sizeof(uint16);  // value len
    len += it->second.length(); // value
    if (it->second.length() == 0) {
      return NULL;
    }
    ++it;
  }
  if (message.tag_value_map.size() % 2 == 1) {
    len += sizeof(uint16);  // padding
  }

  QuicDataWriter writer(len);
  if (!writer.WriteUInt32(message.tag)) {
    DCHECK(false) << "Failed to write message tag.";
    return NULL;
  }
  if (!writer.WriteUInt16(message.tag_value_map.size())) {
    DCHECK(false) << "Failed to write size.";
    return NULL;
  }
  // Tags
  for (it = message.tag_value_map.begin();
       it != message.tag_value_map.end(); ++it) {
    if (!writer.WriteUInt32(it->first)) {
      DCHECK(false) << "Failed to write tag.";
      return NULL;
    }
  }
  // Lengths
  for (it = message.tag_value_map.begin();
       it != message.tag_value_map.end(); ++it) {
    if (!writer.WriteUInt16(it->second.length())) {
      DCHECK(false) << "Failed to write length.";
      return NULL;
    }
  }
  // Possible padding
  if (message.tag_value_map.size() % 2 == 1) {
    if (!writer.WriteUInt16(0xABAB)) {
      DCHECK(false) << "Failed to write padding.";
      return NULL;
    }
  }
  // Values
  for (it = message.tag_value_map.begin();
       it != message.tag_value_map.end(); ++it) {
    if (!writer.WriteBytes(it->second.data(), it->second.length())) {
      DCHECK(false) << "Failed to write value.";
      return NULL;
    }
  }
  return new QuicData(writer.take(), len, true);
}

void CryptoFramer::Clear() {
  tag_value_map_.clear();
  tag_length_map_.clear();
  tags_.clear();
  error_ = QUIC_NO_ERROR;
  state_ = STATE_READING_TAG;
}

}  // namespace net
