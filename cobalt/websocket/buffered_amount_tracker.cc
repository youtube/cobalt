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

#include "cobalt/websocket/buffered_amount_tracker.h"

#include <algorithm>

#include "base/logging.h"

namespace cobalt {
namespace websocket {

std::size_t BufferedAmountTracker::Pop(const std::size_t number_bytes_to_pop) {
  std::size_t size_left_pop = number_bytes_to_pop;
  std::size_t payload_amount = 0;
  while (!entries_.empty() && (size_left_pop > 0)) {
    Entry& entry(entries_[0]);

    std::size_t potential_payload_delta = 0;

    // Cache this variable in case we do a |pop_front|.
    const bool is_user_payload = entry.is_user_payload_;

    if (entry.message_size_ > size_left_pop) {
      potential_payload_delta = size_left_pop;
      entry.message_size_ -= size_left_pop;
    } else {  // entry.message_size <= size_left
      potential_payload_delta += entry.message_size_;
      entries_.pop_front();
    }

    if (is_user_payload) {
      payload_amount += potential_payload_delta;
    }

    size_left_pop -= potential_payload_delta;
  }

  // std::min prevents an underflow due to a bug.
  DCHECK_LE(payload_amount, total_payload_inflight_);

  total_payload_inflight_ -= std::min(total_payload_inflight_, payload_amount);

  // Sort of an overflow check...
  DCHECK_LE(size_left_pop, number_bytes_to_pop);

  return payload_amount;
}

}  // namespace websocket
}  // namespace cobalt
