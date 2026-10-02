// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/segmented_byte_queue.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"

namespace media {

SegmentedByteQueue::SegmentedByteQueue() = default;
SegmentedByteQueue::~SegmentedByteQueue() = default;

void SegmentedByteQueue::Reset() {
  segments_.clear();
  total_bytes_ = 0u;
}

void SegmentedByteQueue::Push(base::span<const uint8_t> data,
                              base::ScopedClosureRunner release) {
  if (data.empty()) {
    return;
  }

  segments_.push_back(Segment{std::move(release), data});
  total_bytes_ += data.size();
}

void SegmentedByteQueue::Pop(size_t count) {
  CHECK_LE(count, total_bytes_);
  total_bytes_ -= count;

  size_t fully_consumed = 0u;
  while (count > 0u) {
    DCHECK_LT(fully_consumed, segments_.size());
    Segment& front = segments_[fully_consumed];
    if (count < front.data.size()) {
      front.data = front.data.subspan(count);
      break;
    }
    count -= front.data.size();

    // Clear the span here rather than leaving it to ~Segment: the erase()
    // below move-assigns the survivors down over these slots, and member-wise
    // move-assignment runs in declaration order, so `release` would free the
    // memory while `data` still pointed at it.
    front.data = {};
    ++fully_consumed;
  }

  segments_.erase(segments_.begin(), segments_.begin() + fully_consumed);
}

base::span<const uint8_t> SegmentedByteQueue::GetContiguousData(
    size_t offset) const {
  if (offset >= total_bytes_) {
    return {};
  }

  auto [index, offset_in_segment] = Locate(offset);
  return segments_[index].data.subspan(offset_in_segment);
}

bool SegmentedByteQueue::GetSegmentedData(
    std::vector<base::span<const uint8_t>>* segments,
    size_t size,
    size_t offset) const {
  DCHECK(segments);

  if (offset > total_bytes_ || total_bytes_ - offset < size) {
    return false;
  }

  segments->clear();
  if (size == 0u) {
    return true;
  }

  auto [index, offset_in_segment] = Locate(offset);
  size_t remaining = size;
  while (remaining > 0u) {
    const base::span<const uint8_t> segment_data = segments_[index].data;
    const size_t taken =
        std::min(remaining, segment_data.size() - offset_in_segment);
    segments->push_back(segment_data.subspan(offset_in_segment, taken));
    remaining -= taken;

    offset_in_segment = 0u;
    ++index;
  }
  return true;
}

std::pair<size_t, size_t> SegmentedByteQueue::Locate(size_t offset) const {
  DCHECK_LT(offset, total_bytes_);

  for (size_t index = 0u; index < segments_.size(); ++index) {
    const size_t segment_size = segments_[index].data.size();
    if (offset < segment_size) {
      return {index, offset};
    }
    offset -= segment_size;
  }

  // Locate() is a private function and the caller ensures `offset` is less
  // than `total_bytes_`, so the offset must have been found above.
  NOTREACHED();
}

}  // namespace media
