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

#include "cobalt/websocket/web_socket_frame_container.h"

namespace {

bool IsFinalChunk(const net::WebSocketFrameChunk* const chunk) {
  return (chunk && chunk->final_chunk);
}

bool IsContinuationOpCode(net::WebSocketFrameHeader::OpCode op_code) {
  switch (op_code) {
    case net::WebSocketFrameHeader::kOpCodeContinuation:
      return true;
    case net::WebSocketFrameHeader::kOpCodeText:
    case net::WebSocketFrameHeader::kOpCodeBinary:
    case net::WebSocketFrameHeader::kOpCodePing:
    case net::WebSocketFrameHeader::kOpCodePong:
    case net::WebSocketFrameHeader::kOpCodeClose:
      return false;
    default:
      NOTREACHED() << "Invalid op_code " << op_code;
  }

  return false;
}

bool IsDataOpCode(net::WebSocketFrameHeader::OpCode op_code) {
  switch (op_code) {
    case net::WebSocketFrameHeader::kOpCodeText:
    case net::WebSocketFrameHeader::kOpCodeBinary:
      return true;
    case net::WebSocketFrameHeader::kOpCodePing:
    case net::WebSocketFrameHeader::kOpCodePong:
    case net::WebSocketFrameHeader::kOpCodeClose:
    case net::WebSocketFrameHeader::kOpCodeContinuation:
      return false;
    default:
      NOTREACHED() << "Invalid op_code " << op_code;
  }

  return false;
}

inline bool IsControlOpCode(net::WebSocketFrameHeader::OpCode op_code) {
  switch (op_code) {
    case net::WebSocketFrameHeader::kOpCodePing:
    case net::WebSocketFrameHeader::kOpCodePong:
    case net::WebSocketFrameHeader::kOpCodeClose:
      return true;
    case net::WebSocketFrameHeader::kOpCodeText:
    case net::WebSocketFrameHeader::kOpCodeBinary:
    case net::WebSocketFrameHeader::kOpCodeContinuation:
      return false;
    default:
      NOTREACHED() << "Invalid op_code " << op_code;
  }

  return false;
}

}  // namespace

namespace cobalt {
namespace websocket {

void WebSocketFrameContainer::clear() {
  for (const_iterator it = chunks_.begin(); it != chunks_.end(); ++it) {
    delete *it;
  }
  chunks_.clear();
  frame_completed_ = false;
  payload_size_bytes_ = 0;
  expected_payload_size_bytes_ = 0;
}

bool WebSocketFrameContainer::IsControlFrame() const {
  const net::WebSocketFrameHeader* header = GetHeader();
  if (!header) {
    return false;
  }
  return IsControlOpCode(header->opcode);
}

bool WebSocketFrameContainer::IsContinuationFrame() const {
  const net::WebSocketFrameHeader* header = GetHeader();
  if (!header) {
    return false;
  }
  return IsContinuationOpCode(header->opcode);
}

bool WebSocketFrameContainer::IsDataFrame() const {
  const net::WebSocketFrameHeader* header = GetHeader();
  if (!header) {
    return false;
  }
  return IsDataOpCode(header->opcode);
}

WebSocketFrameContainer::ErrorCode WebSocketFrameContainer::Take(
    const net::WebSocketFrameChunk* chunk) {
  DCHECK(chunk);

  WebSocketFrameContainer::ErrorCode error_code = kErrorNone;

  do {
    if (IsFrameComplete()) {
      error_code = kErrorFrameAlreadyComplete;
      break;
    }

    const net::WebSocketFrameHeader* const chunk_header = chunk->header.get();

    bool first_chunk = chunks_.empty();
    bool has_frame_header = (chunk_header != NULL);
    if (first_chunk) {
      if (has_frame_header) {
        if (chunk_header->payload_length > kMaxFramePayloadInBytes) {
          error_code = kErrorMaxFrameSizeViolation;
          break;
        } else {
          COMPILE_ASSERT(kuint32max > kMaxFramePayloadInBytes,
                         max_frame_too_big);
          expected_payload_size_bytes_ =
              static_cast<std::size_t>(chunk_header->payload_length);
        }
      } else {
        error_code = kErrorFirstChunkMissingHeader;
        break;
      }
    } else {
      if (has_frame_header) {
        error_code = kErrorHasExtraHeader;
        break;
      }
    }

    // Cases when this should succeed:
    // 1. first_chunk has a header (both are true)
    // 2. non first_chunk has does not have header (both are false)
    // Fun fact: boolean equality is the same as a boolean XNOR.
    DCHECK_EQ(first_chunk, has_frame_header);

    net::IOBufferWithSize* data = chunk->data.get();

    if (data) {
      int chunk_data_size = data->size();
      std::size_t new_payload_size = payload_size_bytes_ + chunk_data_size;

      if (new_payload_size > kMaxFramePayloadInBytes) {
        // This can only happen if the header "lied" about the payload_length.
        error_code = kErrorMaxFrameSizeViolation;
        break;
      }

      if (chunk->final_chunk) {
        if (new_payload_size < expected_payload_size_bytes_) {
          error_code = kErrorPayloadSizeSmallerThanHeader;
          break;
        } else if (new_payload_size > expected_payload_size_bytes_) {
          error_code = kErrorPayloadSizeLargerThanHeader;
          break;
        }
      }

      payload_size_bytes_ += chunk_data_size;
    }

    chunks_.push_back(chunk);
    frame_completed_ |= IsFinalChunk(chunk);
  } while (0);

  if (error_code != kErrorNone) {
    // We didn't take ownership, so let's delete it, so that the caller can
    // always assume that this code takes ownership of the pointer.
    delete chunk;
  }

  return error_code;
}

}  // namespace websocket
}  // namespace cobalt
