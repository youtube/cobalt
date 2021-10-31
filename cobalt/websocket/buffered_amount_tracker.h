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

#ifndef COBALT_WEBSOCKET_BUFFERED_AMOUNT_TRACKER_H_
#define COBALT_WEBSOCKET_BUFFERED_AMOUNT_TRACKER_H_

#include <deque>

namespace cobalt {
namespace websocket {

// This class is helper class for implementing the |bufferedAmount| attribute in
// the Websockets API at:
// https://www.w3.org/TR/websockets/#dom-websocket-bufferedamount.
class BufferedAmountTracker {
 public:
  BufferedAmountTracker() : total_payload_inflight_(0) {}

  // The payload in this context only applies to user messages.
  // So even though a close message might have a "payload", for the purposes
  // of the tracker, the |is_user_payload| will be false.
  void Add(const bool is_user_payload, const std::size_t message_size) {
    if (is_user_payload) {
      total_payload_inflight_ += message_size;
    }
    entries_.push_back(Entry(is_user_payload, message_size));
  }

  // Returns the number of user payload bytes popped, these are bytes that were
  // added with |is_user_payload| set to true.
  std::size_t Pop(const std::size_t number_bytes_to_pop);

  std::size_t GetTotalBytesInflight() const { return total_payload_inflight_; }

 private:
  struct Entry {
    Entry(const bool is_user_payload, const std::size_t message_size)
        : is_user_payload_(is_user_payload), message_size_(message_size) {}

    bool is_user_payload_;
    std::size_t message_size_;
  };

  typedef std::deque<Entry> Entries;

  std::size_t total_payload_inflight_;
  Entries entries_;
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_BUFFERED_AMOUNT_TRACKER_H_
