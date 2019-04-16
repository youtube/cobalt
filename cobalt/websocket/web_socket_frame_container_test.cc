// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "cobalt/websocket/web_socket_frame_container.h"

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace websocket {

class WebSocketFrameContainerTest : public ::testing::Test {
 protected:
  WebSocketFrameContainer frame_container_;
};

TEST_F(WebSocketFrameContainerTest, TestConstruction) {
  EXPECT_TRUE(frame_container_.empty());
  EXPECT_TRUE(frame_container_.begin() == frame_container_.end());
  EXPECT_TRUE(frame_container_.cbegin() == frame_container_.cend());
  EXPECT_EQ(frame_container_.GetChunkCount(), 0UL);
  EXPECT_EQ(frame_container_.GetCurrentPayloadSizeBytes(), 0UL);
  EXPECT_EQ(frame_container_.GetHeader(),
            static_cast<const net::WebSocketFrameHeader*>(NULL));

  EXPECT_FALSE(frame_container_.IsControlFrame());
  EXPECT_FALSE(frame_container_.IsDataFrame());
  EXPECT_FALSE(frame_container_.IsContinuationFrame());

  net::WebSocketFrameHeader::OpCode op = net::WebSocketFrameHeader::kOpCodeText;
  EXPECT_EQ(frame_container_.GetFrameOpCode(&op), false);
}

TEST_F(WebSocketFrameContainerTest, TestClear) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = true;
  chunk1->header->payload_length = 0;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->final_chunk = true;
  WebSocketFrameContainer::ErrorCode error_code1 =
      frame_container_.Take(chunk1);
  EXPECT_EQ(error_code1, WebSocketFrameContainer::kErrorNone);

  frame_container_.clear();

  EXPECT_TRUE(frame_container_.empty());
  EXPECT_TRUE(frame_container_.begin() == frame_container_.end());
  EXPECT_TRUE(frame_container_.cbegin() == frame_container_.cend());
  EXPECT_EQ(frame_container_.GetChunkCount(), 0UL);
  EXPECT_EQ(frame_container_.GetCurrentPayloadSizeBytes(), 0UL);
  EXPECT_EQ(frame_container_.GetHeader(),
            static_cast<const net::WebSocketFrameHeader*>(NULL));

  EXPECT_FALSE(frame_container_.IsControlFrame());
  EXPECT_FALSE(frame_container_.IsDataFrame());
  EXPECT_FALSE(frame_container_.IsContinuationFrame());

  net::WebSocketFrameHeader::OpCode op = net::WebSocketFrameHeader::kOpCodeText;
  EXPECT_EQ(frame_container_.GetFrameOpCode(&op), false);
}

TEST_F(WebSocketFrameContainerTest, TestFirstChunkMissingHeader) {
  net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
  WebSocketFrameContainer::ErrorCode error_code = frame_container_.Take(chunk);
  EXPECT_EQ(error_code, WebSocketFrameContainer::kErrorFirstChunkMissingHeader);
}

TEST_F(WebSocketFrameContainerTest, TestHasExtraHeader) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = true;
  chunk1->header->payload_length = 0;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  WebSocketFrameContainer::ErrorCode error_code1 =
      frame_container_.Take(chunk1);
  EXPECT_EQ(error_code1, WebSocketFrameContainer::kErrorNone);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk2->header->final = true;
  chunk2->header->payload_length = 0;
  chunk2->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  WebSocketFrameContainer::ErrorCode error_code2 =
      frame_container_.Take(chunk2);
  EXPECT_EQ(error_code2, WebSocketFrameContainer::kErrorHasExtraHeader);
}

TEST_F(WebSocketFrameContainerTest, TestFrameAlreadyCompleteNoHeader) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = true;
  chunk1->header->payload_length = 0;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->final_chunk = true;
  WebSocketFrameContainer::ErrorCode error_code1 =
      frame_container_.Take(chunk1);
  EXPECT_EQ(error_code1, WebSocketFrameContainer::kErrorNone);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->final_chunk = true;
  WebSocketFrameContainer::ErrorCode error_code2 =
      frame_container_.Take(chunk2);
  EXPECT_EQ(error_code2, WebSocketFrameContainer::kErrorFrameAlreadyComplete);
}

TEST_F(WebSocketFrameContainerTest, TestFrameAlreadyCompleteHeader) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = true;
  chunk1->header->payload_length = 0;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->final_chunk = true;
  WebSocketFrameContainer::ErrorCode error_code1 =
      frame_container_.Take(chunk1);
  EXPECT_EQ(error_code1, WebSocketFrameContainer::kErrorNone);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk2->header->final = true;
  chunk2->header->payload_length = 0;
  chunk2->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk2->final_chunk = true;
  WebSocketFrameContainer::ErrorCode error_code2 =
      frame_container_.Take(chunk2);
  EXPECT_EQ(error_code2, WebSocketFrameContainer::kErrorFrameAlreadyComplete);
}

TEST_F(WebSocketFrameContainerTest, TestFrameTooBig) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = true;
  chunk1->header->payload_length = 40 * 1024 * 1024;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeText;
  WebSocketFrameContainer::ErrorCode error_code1 =
      frame_container_.Take(chunk1);
  EXPECT_EQ(error_code1, WebSocketFrameContainer::kErrorMaxFrameSizeViolation);
}

TEST_F(WebSocketFrameContainerTest, TestFrameTooBigLieAboutSize) {
  net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
  chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk->header->final = true;
  chunk->header->payload_length = 40;
  chunk->header->opcode = net::WebSocketFrameHeader::kOpCodePing;
  chunk->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(40 * 1024 * 1024));
  WebSocketFrameContainer::ErrorCode error_code = frame_container_.Take(chunk);
  EXPECT_EQ(error_code, WebSocketFrameContainer::kErrorMaxFrameSizeViolation);
}

TEST_F(WebSocketFrameContainerTest, PayloadTooSmall) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 50;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(20));
  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(18));
  EXPECT_EQ(frame_container_.Take(chunk2), WebSocketFrameContainer::kErrorNone);
  net::WebSocketFrameChunk* chunk3 = new net::WebSocketFrameChunk();
  chunk3->final_chunk = true;
  chunk3->data =
      base::WrapRefCounted<net::IOBufferWithSize>(new net::IOBufferWithSize(2));
  EXPECT_EQ(frame_container_.Take(chunk3),
            WebSocketFrameContainer::kErrorPayloadSizeSmallerThanHeader);
}

TEST_F(WebSocketFrameContainerTest, FrameComplete) {
  EXPECT_FALSE(frame_container_.IsFrameComplete());

  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 50;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(20));
  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);
  EXPECT_FALSE(frame_container_.IsFrameComplete());

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(18));
  EXPECT_EQ(frame_container_.Take(chunk2), WebSocketFrameContainer::kErrorNone);
  EXPECT_FALSE(frame_container_.IsFrameComplete());

  net::WebSocketFrameChunk* chunk3 = new net::WebSocketFrameChunk();
  chunk3->final_chunk = true;
  chunk3->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(12));
  EXPECT_EQ(frame_container_.Take(chunk3), WebSocketFrameContainer::kErrorNone);

  EXPECT_TRUE(frame_container_.IsFrameComplete());
}

TEST_F(WebSocketFrameContainerTest, PayloadTooBig) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 50;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(20));
  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(18));
  EXPECT_EQ(frame_container_.Take(chunk2), WebSocketFrameContainer::kErrorNone);

  net::WebSocketFrameChunk* chunk3 = new net::WebSocketFrameChunk();
  chunk3->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(22));
  chunk3->final_chunk = true;
  EXPECT_EQ(frame_container_.Take(chunk3),
            WebSocketFrameContainer::kErrorPayloadSizeLargerThanHeader);
}

TEST_F(WebSocketFrameContainerTest, TestIsControlFrame) {
  struct ControlFrameTestCase {
    net::WebSocketFrameHeader::OpCode op_code;
  } control_frame_test_cases[] = {
      {net::WebSocketFrameHeader::kOpCodePing},
      {net::WebSocketFrameHeader::kOpCodePong},
      {net::WebSocketFrameHeader::kOpCodeClose},
  };

  for (std::size_t i(0); i != ARRAYSIZE_UNSAFE(control_frame_test_cases); ++i) {
    const ControlFrameTestCase& test_case(control_frame_test_cases[i]);
    WebSocketFrameContainer frame_container;
    net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
    chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
        new net::WebSocketFrameHeader());
    chunk->header->final = true;
    chunk->header->payload_length = 0;
    chunk->header->opcode = test_case.op_code;
    EXPECT_EQ(frame_container.Take(chunk), WebSocketFrameContainer::kErrorNone);

    EXPECT_FALSE(frame_container.IsContinuationFrame());
    EXPECT_TRUE(frame_container.IsControlFrame());
    EXPECT_FALSE(frame_container.IsDataFrame());
  }
}

TEST_F(WebSocketFrameContainerTest, TestIsTextDataFrame) {
  net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
  chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk->header->final = true;
  chunk->header->payload_length = 0;
  chunk->header->opcode = net::WebSocketFrameHeader::kOpCodeText;
  EXPECT_EQ(frame_container_.Take(chunk), WebSocketFrameContainer::kErrorNone);

  EXPECT_FALSE(frame_container_.IsContinuationFrame());
  EXPECT_FALSE(frame_container_.IsControlFrame());
  EXPECT_TRUE(frame_container_.IsDataFrame());
}

TEST_F(WebSocketFrameContainerTest, TestIsBinaryDataFrame) {
  net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
  chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk->header->final = true;
  chunk->header->payload_length = 0;
  chunk->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  EXPECT_EQ(frame_container_.Take(chunk), WebSocketFrameContainer::kErrorNone);

  EXPECT_FALSE(frame_container_.IsContinuationFrame());
  EXPECT_FALSE(frame_container_.IsControlFrame());
  EXPECT_TRUE(frame_container_.IsDataFrame());
}

TEST_F(WebSocketFrameContainerTest, TestIsContinuationFrame) {
  net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
  chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk->header->final = true;
  chunk->header->payload_length = 0;
  chunk->header->opcode = net::WebSocketFrameHeader::kOpCodeContinuation;
  EXPECT_EQ(frame_container_.Take(chunk), WebSocketFrameContainer::kErrorNone);

  EXPECT_TRUE(frame_container_.IsContinuationFrame());
  EXPECT_FALSE(frame_container_.IsControlFrame());
  EXPECT_FALSE(frame_container_.IsDataFrame());
}

TEST_F(WebSocketFrameContainerTest, TestIsFrameComplete) {
  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 0;
  chunk1->final_chunk = false;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeContinuation;
  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);

  EXPECT_FALSE(frame_container_.IsFrameComplete());

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->final_chunk = true;
  EXPECT_EQ(frame_container_.Take(chunk2), WebSocketFrameContainer::kErrorNone);

  EXPECT_TRUE(frame_container_.IsFrameComplete());
}

TEST_F(WebSocketFrameContainerTest, TestGetHeader) {
  EXPECT_EQ(frame_container_.GetHeader(),
            static_cast<const net::WebSocketFrameHeader*>(NULL));

  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 0;
  chunk1->final_chunk = false;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeContinuation;

  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);

  EXPECT_NE(frame_container_.GetHeader(),
            static_cast<const net::WebSocketFrameHeader*>(NULL));
}

TEST_F(WebSocketFrameContainerTest, FinalFrameTest) {
  {
    WebSocketFrameContainer frame_container;
    net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
    chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
        new net::WebSocketFrameHeader());
    chunk1->header->final = false;
    chunk1->header->payload_length = 0;
    chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeText;
    EXPECT_EQ(frame_container.Take(chunk1),
              WebSocketFrameContainer::kErrorNone);
    EXPECT_FALSE(frame_container.IsFinalFrame());
  }
  {
    WebSocketFrameContainer frame_container;
    net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
    chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
        new net::WebSocketFrameHeader());
    chunk1->header->final = true;
    chunk1->header->payload_length = 0;
    chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeText;
    EXPECT_EQ(frame_container.Take(chunk1),
              WebSocketFrameContainer::kErrorNone);
    EXPECT_TRUE(frame_container.IsFinalFrame());
  }
}

TEST_F(WebSocketFrameContainerTest, CheckChunkCount) {
  EXPECT_EQ(frame_container_.GetChunkCount(), 0UL);

  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 0;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeText;
  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);

  EXPECT_EQ(frame_container_.GetChunkCount(), 1UL);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  EXPECT_EQ(frame_container_.Take(chunk2), WebSocketFrameContainer::kErrorNone);

  EXPECT_EQ(frame_container_.GetChunkCount(), 2UL);

  net::WebSocketFrameChunk* chunk3 = new net::WebSocketFrameChunk();
  EXPECT_EQ(frame_container_.Take(chunk3), WebSocketFrameContainer::kErrorNone);

  EXPECT_EQ(frame_container_.GetChunkCount(), 3UL);
}

TEST_F(WebSocketFrameContainerTest, CheckPayloadSize) {
  EXPECT_EQ(frame_container_.GetCurrentPayloadSizeBytes(), 0UL);

  net::WebSocketFrameChunk* chunk1 = new net::WebSocketFrameChunk();
  chunk1->header = std::unique_ptr<net::WebSocketFrameHeader>(
      new net::WebSocketFrameHeader());
  chunk1->header->final = false;
  chunk1->header->payload_length = 50;
  chunk1->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
  chunk1->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(20));
  EXPECT_EQ(frame_container_.Take(chunk1), WebSocketFrameContainer::kErrorNone);

  EXPECT_EQ(frame_container_.GetCurrentPayloadSizeBytes(), 20UL);

  net::WebSocketFrameChunk* chunk2 = new net::WebSocketFrameChunk();
  chunk2->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(18));
  EXPECT_EQ(frame_container_.Take(chunk2), WebSocketFrameContainer::kErrorNone);

  EXPECT_EQ(frame_container_.GetCurrentPayloadSizeBytes(), 38UL);

  net::WebSocketFrameChunk* chunk3 = new net::WebSocketFrameChunk();
  chunk3->data = base::WrapRefCounted<net::IOBufferWithSize>(
      new net::IOBufferWithSize(12));
  EXPECT_EQ(frame_container_.Take(chunk3), WebSocketFrameContainer::kErrorNone);

  EXPECT_EQ(frame_container_.GetCurrentPayloadSizeBytes(), 50UL);
}

}  // namespace websocket
}  // namespace cobalt
