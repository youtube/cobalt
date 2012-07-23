// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame_parser.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/big_endian.h"
#include "net/websockets/websocket_frame.h"

namespace {

const uint8 kFinalBit = 0x80;
const uint8 kReserved1Bit = 0x40;
const uint8 kReserved2Bit = 0x20;
const uint8 kReserved3Bit = 0x10;
const uint8 kOpCodeMask = 0xF;
const uint8 kMaskBit = 0x80;
const uint8 kPayloadLengthMask = 0x7F;
const uint64 kMaxPayloadLengthWithoutExtendedLengthField = 125;
const uint64 kPayloadLengthWithTwoByteExtendedLengthField = 126;
const uint64 kPayloadLengthWithEightByteExtendedLengthField = 127;

}  // Unnamed namespace.

namespace net {

WebSocketFrameParser::WebSocketFrameParser()
    : current_read_pos_(0),
      frame_offset_(0),
      failed_(false) {
  std::fill(masking_key_,
            masking_key_ + WebSocketFrameHeader::kMaskingKeyLength,
            '\0');
}

WebSocketFrameParser::~WebSocketFrameParser() {
}

bool WebSocketFrameParser::Decode(
    const char* data,
    size_t length,
    ScopedVector<WebSocketFrameChunk>* frame_chunks) {
  if (failed_)
    return false;
  if (!length)
    return true;

  // TODO(yutak): Remove copy.
  buffer_.insert(buffer_.end(), data, data + length);

  while (current_read_pos_ < buffer_.size()) {
    bool first_chunk = false;
    if (!current_frame_header_.get()) {
      DecodeFrameHeader();
      if (failed_)
        return false;
      // If frame header is incomplete, then carry over the remaining
      // data to the next round of Decode().
      if (!current_frame_header_.get())
        break;
      first_chunk = true;
    }

    scoped_ptr<WebSocketFrameChunk> frame_chunk =
        DecodeFramePayload(first_chunk);
    DCHECK(frame_chunk.get());
    frame_chunks->push_back(frame_chunk.release());

    if (current_frame_header_.get()) {
      DCHECK(current_read_pos_ == buffer_.size());
      break;
    }
  }

  // Drain unnecessary data. TODO(yutak): Remove copy. (but how?)
  buffer_.erase(buffer_.begin(), buffer_.begin() + current_read_pos_);
  current_read_pos_ = 0;

  // Sanity check: the size of carried-over data should not exceed
  // the maximum possible length of a frame header.
  static const size_t kMaximumFrameHeaderSize =
      WebSocketFrameHeader::kBaseHeaderSize +
      WebSocketFrameHeader::kMaximumExtendedLengthSize +
      WebSocketFrameHeader::kMaskingKeyLength;
  DCHECK_LT(buffer_.size(), kMaximumFrameHeaderSize);

  return true;
}

void WebSocketFrameParser::DecodeFrameHeader() {
  typedef WebSocketFrameHeader::OpCode OpCode;
  static const int kMaskingKeyLength = WebSocketFrameHeader::kMaskingKeyLength;

  DCHECK(!current_frame_header_.get());

  const char* start = &buffer_.front() + current_read_pos_;
  const char* current = start;
  const char* end = &buffer_.front() + buffer_.size();

  // Header needs 2 bytes at minimum.
  if (end - current < 2)
    return;

  uint8 first_byte = *current++;
  uint8 second_byte = *current++;

  bool final = (first_byte & kFinalBit) != 0;
  bool reserved1 = (first_byte & kReserved1Bit) != 0;
  bool reserved2 = (first_byte & kReserved2Bit) != 0;
  bool reserved3 = (first_byte & kReserved3Bit) != 0;
  OpCode opcode = first_byte & kOpCodeMask;

  bool masked = (second_byte & kMaskBit) != 0;
  uint64 payload_length = second_byte & kPayloadLengthMask;
  bool valid_length_format = true;
  if (payload_length == kPayloadLengthWithTwoByteExtendedLengthField) {
    if (end - current < 2)
      return;
    uint16 payload_length_16;
    ReadBigEndian(current, &payload_length_16);
    current += 2;
    payload_length = payload_length_16;
    if (payload_length <= kMaxPayloadLengthWithoutExtendedLengthField)
      valid_length_format = false;
  } else if (payload_length == kPayloadLengthWithEightByteExtendedLengthField) {
    if (end - current < 8)
      return;
    ReadBigEndian(current, &payload_length);
    current += 8;
    if (payload_length <= kuint16max ||
        payload_length > static_cast<uint64>(kint64max)) {
      valid_length_format = false;
    }
  }
  if (!valid_length_format) {
    failed_ = true;
    buffer_.clear();
    current_read_pos_ = 0;
    current_frame_header_.reset();
    frame_offset_ = 0;
    return;
  }

  if (masked) {
    if (end - current < kMaskingKeyLength)
      return;
    std::copy(current, current + kMaskingKeyLength, masking_key_);
    current += kMaskingKeyLength;
  } else {
    std::fill(masking_key_, masking_key_ + kMaskingKeyLength, '\0');
  }

  current_frame_header_.reset(new WebSocketFrameHeader);
  current_frame_header_->final = final;
  current_frame_header_->reserved1 = reserved1;
  current_frame_header_->reserved2 = reserved2;
  current_frame_header_->reserved3 = reserved3;
  current_frame_header_->opcode = opcode;
  current_frame_header_->masked = masked;
  current_frame_header_->payload_length = payload_length;
  current_read_pos_ += current - start;
  DCHECK_EQ(0u, frame_offset_);
}

scoped_ptr<WebSocketFrameChunk> WebSocketFrameParser::DecodeFramePayload(
    bool first_chunk) {
  static const int kMaskingKeyLength = WebSocketFrameHeader::kMaskingKeyLength;

  const char* current = &buffer_.front() + current_read_pos_;
  const char* end = &buffer_.front() + buffer_.size();
  uint64 next_size = std::min<uint64>(
      end - current,
      current_frame_header_->payload_length - frame_offset_);

  scoped_ptr<WebSocketFrameChunk> frame_chunk(new WebSocketFrameChunk);
  if (first_chunk) {
    frame_chunk->header.reset(new WebSocketFrameHeader(*current_frame_header_));
  }
  frame_chunk->final_chunk = false;
  frame_chunk->data.assign(current, current + next_size);
  if (current_frame_header_->masked) {
    // Unmask the payload.
    // TODO(yutak): This could be faster by doing unmasking for each
    // machine word (instead of each byte).
    size_t key_offset = frame_offset_ % kMaskingKeyLength;
    for (uint64 i = 0; i < next_size; ++i) {
      frame_chunk->data[i] ^= masking_key_[key_offset];
      key_offset = (key_offset + 1) % kMaskingKeyLength;
    }
  }

  current_read_pos_ += next_size;
  frame_offset_ += next_size;

  DCHECK_LE(frame_offset_, current_frame_header_->payload_length);
  if (frame_offset_ == current_frame_header_->payload_length) {
    frame_chunk->final_chunk = true;
    current_frame_header_.reset();
    frame_offset_ = 0;
  }

  return frame_chunk.Pass();
}

}  // namespace net
