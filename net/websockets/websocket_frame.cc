// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "net/base/big_endian.h"
#include "net/base/net_errors.h"

namespace {

const uint8 kFinalBit = 0x80;
const uint8 kReserved1Bit = 0x40;
const uint8 kReserved2Bit = 0x20;
const uint8 kReserved3Bit = 0x10;
const uint8 kOpCodeMask = 0xF;
const uint8 kMaskBit = 0x80;
const uint64 kMaxPayloadLengthWithoutExtendedLengthField = 125;
const uint64 kPayloadLengthWithTwoByteExtendedLengthField = 126;
const uint64 kPayloadLengthWithEightByteExtendedLengthField = 127;

}  // Unnamed namespace.

namespace net {

// Definitions for in-struct constants.
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeContinuation =
    0x0;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeText = 0x1;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeBinary = 0x2;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeClose = 0x8;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodePing = 0x9;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodePong = 0xA;

WebSocketFrameChunk::WebSocketFrameChunk() : final_chunk(false) {
}

WebSocketFrameChunk::~WebSocketFrameChunk() {
}

int WriteWebSocketFrameHeader(const WebSocketFrameHeader& header,
                              const WebSocketMaskingKey* masking_key,
                              char* buffer,
                              int buffer_size) {
  DCHECK((header.opcode & kOpCodeMask) == header.opcode)
      << "header.opcode must fit to kOpCodeMask.";
  DCHECK(header.payload_length <= static_cast<uint64>(kint64max))
      << "WebSocket specification doesn't allow a frame longer than "
      << "kint64max (0x7FFFFFFFFFFFFFFF) bytes.";
  DCHECK_GE(buffer_size, 0);

  // WebSocket frame format is as follows:
  // - Common header (2 bytes)
  // - Optional extended payload length
  //   (2 or 8 bytes, present if actual payload length is more than 125 bytes)
  // - Optional masking key (4 bytes, present if MASK bit is on)
  // - Actual payload (XOR masked with masking key if MASK bit is on)
  //
  // This function constructs frame header (the first three in the list
  // above).

  size_t extended_length_size = 0;
  if (header.payload_length > kMaxPayloadLengthWithoutExtendedLengthField &&
      header.payload_length <= kuint16max) {
    extended_length_size = 2;
  } else if (header.payload_length > kuint16max) {
    extended_length_size = 8;
  }
  int header_size =
      WebSocketFrameHeader::kBaseHeaderSize +
      extended_length_size +
      (header.masked ? WebSocketFrameHeader::kMaskingKeyLength : 0);
  if (header_size > buffer_size)
    return ERR_INVALID_ARGUMENT;

  int buffer_index = 0;

  uint8 first_byte = 0u;
  first_byte |= header.final ? kFinalBit : 0u;
  first_byte |= header.reserved1 ? kReserved1Bit : 0u;
  first_byte |= header.reserved2 ? kReserved2Bit : 0u;
  first_byte |= header.reserved3 ? kReserved3Bit : 0u;
  first_byte |= header.opcode & kOpCodeMask;
  buffer[buffer_index++] = first_byte;

  uint8 second_byte = 0u;
  second_byte |= header.masked ? kMaskBit : 0u;
  if (header.payload_length <=
      kMaxPayloadLengthWithoutExtendedLengthField) {
    second_byte |= header.payload_length;
  } else if (header.payload_length <= kuint16max) {
    second_byte |= kPayloadLengthWithTwoByteExtendedLengthField;
  } else {
    second_byte |= kPayloadLengthWithEightByteExtendedLengthField;
  }
  buffer[buffer_index++] = second_byte;

  // Writes "extended payload length" field.
  if (extended_length_size == 2u) {
    uint16 payload_length_16 = static_cast<uint16>(header.payload_length);
    WriteBigEndian(buffer + buffer_index, payload_length_16);
    buffer_index += sizeof(uint16);
  } else if (extended_length_size == 8u) {
    WriteBigEndian(buffer + buffer_index, header.payload_length);
    buffer_index += sizeof(uint64);
  }

  // Writes "masking key" field, if needed.
  if (header.masked) {
    DCHECK(masking_key);
    std::copy(masking_key->key,
              masking_key->key + WebSocketFrameHeader::kMaskingKeyLength,
              buffer + buffer_index);
    buffer_index += WebSocketFrameHeader::kMaskingKeyLength;
  } else {
    DCHECK(!masking_key);
  }

  DCHECK_EQ(header_size, buffer_index);
  return header_size;
}

WebSocketMaskingKey GenerateWebSocketMaskingKey() {
  // Masking keys should be generated from a cryptographically secure random
  // number generator, which means web application authors should not be able
  // to guess the next value of masking key.
  WebSocketMaskingKey masking_key;
  base::RandBytes(masking_key.key, WebSocketFrameHeader::kMaskingKeyLength);
  return masking_key;
}

void MaskWebSocketFramePayload(const WebSocketMaskingKey& masking_key,
                               uint64 frame_offset,
                               char* data,
                               int data_size) {
  static const size_t kMaskingKeyLength =
      WebSocketFrameHeader::kMaskingKeyLength;

  DCHECK_GE(data_size, 0);

  // TODO(yutak): Make masking more efficient by XOR'ing every machine word
  // (4 or 8 bytes), instead of XOR'ing every byte.
  size_t masking_key_offset = frame_offset % kMaskingKeyLength;
  for (int i = 0; i < data_size; ++i) {
    data[i] ^= masking_key.key[masking_key_offset++];
    if (masking_key_offset == kMaskingKeyLength)
      masking_key_offset = 0;
  }
}

}  // namespace net
