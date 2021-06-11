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

#include "cobalt/websocket/web_socket_message_container.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char payload_filler = '?';

}  // namespace

namespace cobalt {
namespace websocket {

class WebSocketMessageContainerTest : public ::testing::Test {
 public:
  WebSocketMessageContainerTest() {
    PopulateBinaryFrame(2);  // Create a final binary frame with 2 byte payload.
    PopulateTextFrame();
    PopulateContinuationFrame(3);
  }

 protected:
  void PopulateBinaryFrame(int payload_size) {
    net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
    chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
        new net::WebSocketFrameHeader());
    chunk->header->final = true;
    chunk->header->payload_length = payload_size;
    chunk->header->opcode = net::WebSocketFrameHeader::kOpCodeBinary;
    chunk->data = base::WrapRefCounted<net::IOBufferWithSize>(
        new net::IOBufferWithSize(payload_size));
    chunk->final_chunk = true;
    WebSocketFrameContainer::ErrorCode error_code =
        final_binary_frame_.Take(chunk);
    EXPECT_EQ(error_code, WebSocketFrameContainer::kErrorNone);
  }

  void PopulateTextFrame() {
    net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
    chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
        new net::WebSocketFrameHeader());
    chunk->header->final = false;
    chunk->header->payload_length = 0;
    chunk->header->opcode = net::WebSocketFrameHeader::kOpCodeText;
    chunk->final_chunk = true;
    WebSocketFrameContainer::ErrorCode error_code =
        nonfinal_text_frame_.Take(chunk);
    EXPECT_EQ(error_code, WebSocketFrameContainer::kErrorNone);
  }

  void PopulateContinuationFrame(int payload_size) {
    net::WebSocketFrameChunk* chunk = new net::WebSocketFrameChunk();
    chunk->header = std::unique_ptr<net::WebSocketFrameHeader>(
        new net::WebSocketFrameHeader());
    chunk->header->final = true;
    chunk->header->payload_length = payload_size;
    chunk->header->opcode = net::WebSocketFrameHeader::kOpCodeContinuation;
    chunk->data = base::WrapRefCounted<net::IOBufferWithSize>(
        new net::IOBufferWithSize(payload_size));

    net::IOBufferWithSize& data_array(*chunk->data.get());
    memset(data_array.data(), payload_filler, data_array.size());

    chunk->final_chunk = true;
    WebSocketFrameContainer::ErrorCode error_code =
        final_continuation_frame_.Take(chunk);
    EXPECT_EQ(error_code, WebSocketFrameContainer::kErrorNone);
  }

  WebSocketFrameContainer final_binary_frame_;  // Final frame

  WebSocketFrameContainer nonfinal_text_frame_;  // Not a final frame.
  WebSocketFrameContainer final_continuation_frame_;

  WebSocketMessageContainer message_container_;
};

TEST_F(WebSocketMessageContainerTest, Construct) {
  EXPECT_TRUE(message_container_.empty());
  EXPECT_FALSE(message_container_.IsMessageComplete());
  EXPECT_EQ(message_container_.GetCurrentPayloadSizeBytes(), 0UL);
}

TEST_F(WebSocketMessageContainerTest, Clear) {
  EXPECT_TRUE(message_container_.Take(&final_binary_frame_));
  message_container_.clear();
  EXPECT_TRUE(message_container_.empty());
  EXPECT_FALSE(message_container_.IsMessageComplete());
  EXPECT_EQ(message_container_.GetCurrentPayloadSizeBytes(), 0UL);
}

TEST_F(WebSocketMessageContainerTest, GetMessageOpCode) {
  net::WebSocketFrameHeader::OpCode op;
  EXPECT_FALSE(message_container_.GetMessageOpCode(&op));
  EXPECT_TRUE(message_container_.Take(&final_binary_frame_));
  EXPECT_TRUE(message_container_.GetMessageOpCode(&op));
  EXPECT_EQ(op, net::WebSocketFrameHeader::kOpCodeBinary);
}

TEST_F(WebSocketMessageContainerTest, TakeMultipleFrames) {
  EXPECT_EQ(message_container_.GetFrames().size(), 0U);
  EXPECT_TRUE(message_container_.Take(&nonfinal_text_frame_));
  EXPECT_EQ(message_container_.GetFrames().size(), 1U);
  EXPECT_TRUE(message_container_.Take(&final_continuation_frame_));
  EXPECT_EQ(message_container_.GetFrames().size(), 2U);
}

TEST_F(WebSocketMessageContainerTest, IsMessageComplete) {
  EXPECT_TRUE(message_container_.Take(&final_binary_frame_));
  EXPECT_TRUE(message_container_.IsMessageComplete());
}

TEST_F(WebSocketMessageContainerTest, IsMessageComplete2) {
  EXPECT_TRUE(message_container_.Take(&nonfinal_text_frame_));
  EXPECT_FALSE(message_container_.IsMessageComplete());
  EXPECT_TRUE(message_container_.Take(&final_continuation_frame_));
  EXPECT_TRUE(message_container_.IsMessageComplete());
}

TEST_F(WebSocketMessageContainerTest, IsTextMessage) {
  EXPECT_TRUE(message_container_.Take(&nonfinal_text_frame_));
  EXPECT_TRUE(message_container_.IsTextMessage());
}

TEST_F(WebSocketMessageContainerTest, IsTextMessage2) {
  EXPECT_TRUE(message_container_.Take(&final_binary_frame_));
  EXPECT_FALSE(message_container_.IsTextMessage());
}

TEST_F(WebSocketMessageContainerTest, PayloadSize) {
  EXPECT_EQ(message_container_.GetCurrentPayloadSizeBytes(), 0UL);
  EXPECT_TRUE(message_container_.Take(&nonfinal_text_frame_));
  EXPECT_EQ(message_container_.GetCurrentPayloadSizeBytes(), 0UL);
  EXPECT_TRUE(message_container_.Take(&final_continuation_frame_));
  EXPECT_EQ(message_container_.GetCurrentPayloadSizeBytes(), 3UL);
}

TEST_F(WebSocketMessageContainerTest, TakeStartsWithContinuationFrame) {
  EXPECT_FALSE(message_container_.Take(&final_continuation_frame_));
}

TEST_F(WebSocketMessageContainerTest, GetIOBuffer) {
  EXPECT_TRUE(message_container_.Take(&nonfinal_text_frame_));
  EXPECT_TRUE(message_container_.Take(&final_continuation_frame_));
  scoped_refptr<net::IOBufferWithSize> payload =
      message_container_.GetMessageAsIOBuffer();

  int payload_size = payload->size();
  EXPECT_GE(payload_size, 0);

  char* data = payload->data();
  for (int i = 0; i != payload_size; ++i) {
    DCHECK_EQ(payload_filler, data[i]);
  }
}

}  // namespace websocket
}  // namespace cobalt
