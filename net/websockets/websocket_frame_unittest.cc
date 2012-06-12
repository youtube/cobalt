// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include <vector>

#include "base/basictypes.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(WebSocketFrameHeaderTest, FrameLengths) {
  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
    uint64 frame_length;
  };
  static const TestCase kTests[] = {
    { "\x81\x00", 2, GG_UINT64_C(0) },
    { "\x81\x7D", 2, GG_UINT64_C(125) },
    { "\x81\x7E\x00\x7E", 4, GG_UINT64_C(126) },
    { "\x81\x7E\xFF\xFF", 4, GG_UINT64_C(0xFFFF) },
    { "\x81\x7F\x00\x00\x00\x00\x00\x01\x00\x00", 10, GG_UINT64_C(0x10000) },
    { "\x81\x7F\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 10,
      GG_UINT64_C(0x7FFFFFFFFFFFFFFF) }
  };
  static const int kNumTests = ARRAYSIZE_UNSAFE(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    WebSocketFrameHeader header;
    header.final = true;
    header.reserved1 = false;
    header.reserved2 = false;
    header.reserved3 = false;
    header.opcode = WebSocketFrameHeader::kOpCodeText;
    header.masked = false;
    header.payload_length = kTests[i].frame_length;

    std::vector<char> expected_output(
        kTests[i].frame_header,
        kTests[i].frame_header + kTests[i].frame_header_length);
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, NULL, &output.front(),
                                        output.size()));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, FrameLengthsWithMasking) {
  static const char kMaskingKey[] = "\xDE\xAD\xBE\xEF";
  COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kMaskingKey) - 1 ==
                 WebSocketFrameHeader::kMaskingKeyLength,
                 incorrect_masking_key_size);

  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
    uint64 frame_length;
  };
  static const TestCase kTests[] = {
    { "\x81\x80\xDE\xAD\xBE\xEF", 6, GG_UINT64_C(0) },
    { "\x81\xFD\xDE\xAD\xBE\xEF", 6, GG_UINT64_C(125) },
    { "\x81\xFE\x00\x7E\xDE\xAD\xBE\xEF", 8, GG_UINT64_C(126) },
    { "\x81\xFE\xFF\xFF\xDE\xAD\xBE\xEF", 8, GG_UINT64_C(0xFFFF) },
    { "\x81\xFF\x00\x00\x00\x00\x00\x01\x00\x00\xDE\xAD\xBE\xEF", 14,
      GG_UINT64_C(0x10000) },
    { "\x81\xFF\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xDE\xAD\xBE\xEF", 14,
      GG_UINT64_C(0x7FFFFFFFFFFFFFFF) }
  };
  static const int kNumTests = ARRAYSIZE_UNSAFE(kTests);

  WebSocketMaskingKey masking_key;
  std::copy(kMaskingKey, kMaskingKey + WebSocketFrameHeader::kMaskingKeyLength,
            masking_key.key);

  for (int i = 0; i < kNumTests; ++i) {
    WebSocketFrameHeader header;
    header.final = true;
    header.reserved1 = false;
    header.reserved2 = false;
    header.reserved3 = false;
    header.opcode = WebSocketFrameHeader::kOpCodeText;
    header.masked = true;
    header.payload_length = kTests[i].frame_length;

    std::vector<char> expected_output(
        kTests[i].frame_header,
        kTests[i].frame_header + kTests[i].frame_header_length);
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, &masking_key,
                                        &output.front(), output.size()));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, FrameOpCodes) {
  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
    WebSocketFrameHeader::OpCode opcode;
  };
  static const TestCase kTests[] = {
    { "\x80\x00", 2, WebSocketFrameHeader::kOpCodeContinuation },
    { "\x81\x00", 2, WebSocketFrameHeader::kOpCodeText },
    { "\x82\x00", 2, WebSocketFrameHeader::kOpCodeBinary },
    { "\x88\x00", 2, WebSocketFrameHeader::kOpCodeClose },
    { "\x89\x00", 2, WebSocketFrameHeader::kOpCodePing },
    { "\x8A\x00", 2, WebSocketFrameHeader::kOpCodePong },
    // These are undefined opcodes, but the builder should accept them anyway.
    { "\x83\x00", 2, 0x3 },
    { "\x84\x00", 2, 0x4 },
    { "\x85\x00", 2, 0x5 },
    { "\x86\x00", 2, 0x6 },
    { "\x87\x00", 2, 0x7 },
    { "\x8B\x00", 2, 0xB },
    { "\x8C\x00", 2, 0xC },
    { "\x8D\x00", 2, 0xD },
    { "\x8E\x00", 2, 0xE },
    { "\x8F\x00", 2, 0xF }
  };
  static const int kNumTests = ARRAYSIZE_UNSAFE(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    WebSocketFrameHeader header;
    header.final = true;
    header.reserved1 = false;
    header.reserved2 = false;
    header.reserved3 = false;
    header.opcode = kTests[i].opcode;
    header.masked = false;
    header.payload_length = 0;

    std::vector<char> expected_output(
        kTests[i].frame_header,
        kTests[i].frame_header + kTests[i].frame_header_length);
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, NULL,
                                        &output.front(), output.size()));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, FinalBitAndReservedBits) {
  struct TestCase {
    const char* frame_header;
    size_t frame_header_length;
    bool final;
    bool reserved1;
    bool reserved2;
    bool reserved3;
  };
  static const TestCase kTests[] = {
    { "\x81\x00", 2, true, false, false, false },
    { "\x01\x00", 2, false, false, false, false },
    { "\xC1\x00", 2, true, true, false, false },
    { "\xA1\x00", 2, true, false, true, false },
    { "\x91\x00", 2, true, false, false, true },
    { "\x71\x00", 2, false, true, true, true },
    { "\xF1\x00", 2, true, true, true, true }
  };
  static const int kNumTests = ARRAYSIZE_UNSAFE(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    WebSocketFrameHeader header;
    header.final = kTests[i].final;
    header.reserved1 = kTests[i].reserved1;
    header.reserved2 = kTests[i].reserved2;
    header.reserved3 = kTests[i].reserved3;
    header.opcode = WebSocketFrameHeader::kOpCodeText;
    header.masked = false;
    header.payload_length = 0;

    std::vector<char> expected_output(
        kTests[i].frame_header,
        kTests[i].frame_header + kTests[i].frame_header_length);
    std::vector<char> output(expected_output.size());
    EXPECT_EQ(static_cast<int>(expected_output.size()),
              WriteWebSocketFrameHeader(header, NULL,
                                        &output.front(), output.size()));
    EXPECT_EQ(expected_output, output);
  }
}

TEST(WebSocketFrameHeaderTest, InsufficientBufferSize) {
  struct TestCase {
    uint64 payload_length;
    bool masked;
    size_t expected_header_size;
  };
  static const TestCase kTests[] = {
    { GG_UINT64_C(0), false, 2u },
    { GG_UINT64_C(125), false, 2u },
    { GG_UINT64_C(126), false, 4u },
    { GG_UINT64_C(0xFFFF), false, 4u },
    { GG_UINT64_C(0x10000), false, 10u },
    { GG_UINT64_C(0x7FFFFFFFFFFFFFFF), false, 10u },
    { GG_UINT64_C(0), true, 6u },
    { GG_UINT64_C(125), true, 6u },
    { GG_UINT64_C(126), true, 8u },
    { GG_UINT64_C(0xFFFF), true, 8u },
    { GG_UINT64_C(0x10000), true, 14u },
    { GG_UINT64_C(0x7FFFFFFFFFFFFFFF), true, 14u }
  };
  static const int kNumTests = ARRAYSIZE_UNSAFE(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    WebSocketFrameHeader header;
    header.final = true;
    header.reserved1 = false;
    header.reserved2 = false;
    header.reserved3 = false;
    header.opcode = WebSocketFrameHeader::kOpCodeText;
    header.masked = kTests[i].masked;
    header.payload_length = kTests[i].payload_length;

    char dummy_buffer[14];
    // Set an insufficient size to |buffer_size|.
    EXPECT_EQ(ERR_INVALID_ARGUMENT,
              WriteWebSocketFrameHeader(header, NULL, dummy_buffer,
                                        kTests[i].expected_header_size - 1));
  }
}

TEST(WebSocketFrameTest, MaskPayload) {
  struct TestCase {
    const char* masking_key;
    uint64 frame_offset;
    const char* input;
    const char* output;
    size_t data_length;
  };
  static const TestCase kTests[] = {
    { "\xDE\xAD\xBE\xEF", 0, "FooBar", "\x98\xC2\xD1\xAD\xBF\xDF", 6 },
    { "\xDE\xAD\xBE\xEF", 1, "FooBar", "\xEB\xD1\x80\x9C\xCC\xCC", 6 },
    { "\xDE\xAD\xBE\xEF", 2, "FooBar", "\xF8\x80\xB1\xEF\xDF\x9D", 6 },
    { "\xDE\xAD\xBE\xEF", 3, "FooBar", "\xA9\xB1\xC2\xFC\x8E\xAC", 6 },
    { "\xDE\xAD\xBE\xEF", 4, "FooBar", "\x98\xC2\xD1\xAD\xBF\xDF", 6 },
    { "\xDE\xAD\xBE\xEF", 42, "FooBar", "\xF8\x80\xB1\xEF\xDF\x9D", 6 },
    { "\xDE\xAD\xBE\xEF", 0, "", "", 0 },
    { "\xDE\xAD\xBE\xEF", 0, "\xDE\xAD\xBE\xEF", "\x00\x00\x00\x00", 4 },
    { "\xDE\xAD\xBE\xEF", 0, "\x00\x00\x00\x00", "\xDE\xAD\xBE\xEF", 4 },
    { "\x00\x00\x00\x00", 0, "FooBar", "FooBar", 6 },
    { "\xFF\xFF\xFF\xFF", 0, "FooBar", "\xB9\x90\x90\xBD\x9E\x8D", 6 },
  };
  static const int kNumTests = ARRAYSIZE_UNSAFE(kTests);

  for (int i = 0; i < kNumTests; ++i) {
    WebSocketMaskingKey masking_key;
    std::copy(kTests[i].masking_key,
              kTests[i].masking_key + WebSocketFrameHeader::kMaskingKeyLength,
              masking_key.key);
    std::vector<char> frame_data(kTests[i].input,
                                 kTests[i].input + kTests[i].data_length);
    std::vector<char> expected_output(kTests[i].output,
                                      kTests[i].output + kTests[i].data_length);
    MaskWebSocketFramePayload(masking_key,
                              kTests[i].frame_offset,
                              frame_data.empty() ? NULL : &frame_data.front(),
                              frame_data.size());
    EXPECT_EQ(expected_output, frame_data);
  }
}

}  // namespace net
