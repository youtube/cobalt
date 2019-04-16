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
#ifndef COBALT_WEBSOCKET_WEB_SOCKET_FRAME_CONTAINER_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_FRAME_CONTAINER_H_

#include <algorithm>
#include <deque>
#include <memory>

#include "base/basictypes.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_frame.h"

namespace cobalt {
namespace websocket {

const size_t kMaxFramePayloadInBytes = 4 * 1024 * 1024;

class WebSocketFrameContainer {
 public:
  typedef std::deque<const net::WebSocketFrameChunk*> WebSocketFrameChunks;
  typedef WebSocketFrameChunks::iterator iterator;
  typedef WebSocketFrameChunks::const_iterator const_iterator;

  WebSocketFrameContainer()
      : frame_completed_(false),
        payload_size_bytes_(0),
        expected_payload_size_bytes_(0) {}
  ~WebSocketFrameContainer() { clear(); }

  void clear();

  const net::WebSocketFrameHeader* GetHeader() const {
    if (empty()) {
      return NULL;
    }

    return (*begin())->header.get();
  }

  bool IsControlFrame() const;
  bool IsDataFrame() const;
  bool IsContinuationFrame() const;

  enum ErrorCode {
    kErrorNone,
    kErrorMaxFrameSizeViolation,
    kErrorFirstChunkMissingHeader,
    kErrorHasExtraHeader,
    kErrorFrameAlreadyComplete,
    kErrorPayloadSizeSmallerThanHeader,
    kErrorPayloadSizeLargerThanHeader
  };

  bool IsFrameComplete() const { return frame_completed_; }

  bool IsFinalFrame() const {
    const net::WebSocketFrameHeader* const header = GetHeader();
    if (!header) {
      return false;
    }
    return (header && header->final);
  }

  // Note that this takes always ownership of the chunk.
  // Should only be called if IsFrameComplete() is false.
  // Note that if there is an error produced in the function, it will
  // leave the state of this object unchanged.
  ErrorCode Take(const net::WebSocketFrameChunk* chunk);

  iterator begin() { return chunks_.begin(); }
  iterator end() { return chunks_.end(); }
  const_iterator begin() const { return chunks_.begin(); }
  const_iterator end() const { return chunks_.end(); }
  const_iterator cbegin() const { return chunks_.begin(); }
  const_iterator cend() const { return chunks_.end(); }
  bool empty() const { return begin() == end(); }

  std::size_t GetCurrentPayloadSizeBytes() const { return payload_size_bytes_; }
  std::size_t GetChunkCount() const { return chunks_.size(); }

  void swap(WebSocketFrameContainer& other) {
    std::swap(frame_completed_, other.frame_completed_);
    std::swap(payload_size_bytes_, other.payload_size_bytes_);
    std::swap(expected_payload_size_bytes_, other.expected_payload_size_bytes_);
    chunks_.swap(other.chunks_);
  }

  // Returns false if op_code was not found, and returns true otherwise.
  bool GetFrameOpCode(net::WebSocketFrameHeader::OpCode* op_code) const {
    DCHECK(op_code);
    if (empty()) {
      return false;
    }

    const net::WebSocketFrameChunk* first_chunk(*begin());
    DCHECK(first_chunk);
    const std::unique_ptr<net::WebSocketFrameHeader>& first_chunk_header =
        first_chunk->header;
    if (!first_chunk_header) {
      NOTREACHED() << "No header found in the first chunk.";
      return false;
    }

    *op_code = first_chunk_header->opcode;
    return true;
  }

 private:
  // Note: If you add a field, please remember to update swap() above.
  bool frame_completed_;
  std::size_t payload_size_bytes_;
  std::size_t expected_payload_size_bytes_;
  WebSocketFrameChunks chunks_;
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_FRAME_CONTAINER_H_
