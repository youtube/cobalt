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

#include "cobalt/websocket/web_socket_message_container.h"

#include "base/basictypes.h"

namespace cobalt {
namespace websocket {

std::size_t CombineFramesChunks(WebSocketFrameContainer::const_iterator begin,
                                WebSocketFrameContainer::const_iterator end,
                                char *out_destination,
                                std::size_t buffer_length) {
  DCHECK(out_destination);
  std::size_t bytes_written = 0;
  std::size_t bytes_available = buffer_length;
  for (WebSocketFrameContainer::const_iterator iterator = begin;
       iterator != end; ++iterator) {
    const scoped_refptr<net::IOBufferWithSize> &data((*iterator)->data);

    if (data) {
      std::size_t frame_chunk_size = data->size();

      if (bytes_available >= frame_chunk_size) {
        memcpy(out_destination, data->data(), frame_chunk_size);
        out_destination += frame_chunk_size;
        bytes_written += frame_chunk_size;
        bytes_available -= frame_chunk_size;
      }
    }
  }

  DCHECK_LE(bytes_written, buffer_length);
  return bytes_written;
}

bool WebSocketMessageContainer::Take(WebSocketFrameContainer *frame_container) {
  DCHECK(frame_container);
  DCHECK(!IsMessageComplete());
  DCHECK(frame_container->IsFrameComplete());
  if (!frame_container) {
    return false;
  }
  if (frame_container->empty()) {
    return true;
  }

  bool is_first_frame = frames_.empty();
  bool is_continuation_frame = frame_container->IsContinuationFrame();

  if (is_first_frame) {
    if (is_continuation_frame) {
      return false;
    }
  } else {
    // All frames after the first one must be continuation frames.
    if (!is_continuation_frame) {
      return false;
    }
  }

  frames_.push_back(WebSocketFrameContainer());

  WebSocketFrameContainer &last_object(frames_.back());
  last_object.swap(*frame_container);
  DCHECK(last_object.IsFrameComplete());

  payload_size_bytes_ += last_object.GetCurrentPayloadSizeBytes();
  message_completed_ |= last_object.IsFinalFrame();

  return true;
}

scoped_refptr<net::IOBufferWithSize>
WebSocketMessageContainer::GetMessageAsIOBuffer() const {
  scoped_refptr<net::IOBufferWithSize> buf;

  DCHECK_LE(kMaxMessagePayloadInBytes, static_cast<std::size_t>(kint32max));
  DCHECK_LE(payload_size_bytes_, kMaxMessagePayloadInBytes);
  DCHECK_GE(payload_size_bytes_, 0UL);

  if ((payload_size_bytes_ > 0) &&
      (payload_size_bytes_ <= kMaxMessagePayloadInBytes)) {
    buf = base::WrapRefCounted(
        new net::IOBufferWithSize(static_cast<int>(payload_size_bytes_)));

    std::size_t total_bytes_written = 0;

    char *data_pointer = buf->data();
    std::size_t size_remaining = buf->size();

    for (WebSocketFrames::const_iterator iterator = frames_.begin();
         iterator != frames_.end(); ++iterator) {
      const WebSocketFrameContainer &frame_container(*iterator);

      std::size_t bytes_written =
          CombineFramesChunks(frame_container.begin(), frame_container.end(),
                              data_pointer, size_remaining);

      DCHECK_LE(bytes_written, size_remaining);

      size_remaining -= bytes_written;
      data_pointer += bytes_written;

      total_bytes_written += bytes_written;
    }

    DCHECK_EQ(total_bytes_written, payload_size_bytes_);
  }

  return buf;
}

}  // namespace websocket
}  // namespace cobalt
