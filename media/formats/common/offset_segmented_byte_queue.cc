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

#include "media/formats/common/offset_segmented_byte_queue.h"

#include <utility>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"

namespace media {

OffsetSegmentedByteQueue::OffsetSegmentedByteQueue() = default;
OffsetSegmentedByteQueue::~OffsetSegmentedByteQueue() = default;

void OffsetSegmentedByteQueue::Reset() {
  queue_.Reset();
  head_ = 0;
}

void OffsetSegmentedByteQueue::Push(base::span<const uint8_t> data,
                                    base::ScopedClosureRunner release) {
  queue_.Push(data, std::move(release));
}

void OffsetSegmentedByteQueue::Pop(size_t count) {
  queue_.Pop(count);
  head_ += base::checked_cast<int64_t>(count);
}

bool OffsetSegmentedByteQueue::Trim(int64_t max_offset) {
  if (max_offset < head_) {
    return true;
  }
  if (max_offset > tail()) {
    Pop(queue_.size());
    return false;
  }
  Pop(base::checked_cast<size_t>(max_offset - head_));
  return true;
}

base::span<const uint8_t> OffsetSegmentedByteQueue::GetContiguousData(
    int64_t offset) const {
  if (!IsReadableOffset(offset)) {
    return {};
  }
  return queue_.GetContiguousData(OffsetFromHead(offset));
}

bool OffsetSegmentedByteQueue::GetSegmentedData(
    std::vector<base::span<const uint8_t>>* segments,
    size_t size,
    int64_t offset) const {
  if (!IsReadableOffset(offset)) {
    return false;
  }
  return queue_.GetSegmentedData(segments, size, OffsetFromHead(offset));
}

size_t OffsetSegmentedByteQueue::OffsetFromHead(int64_t offset) const {
  DCHECK(IsReadableOffset(offset));
  return base::checked_cast<size_t>(offset - head_);
}

}  // namespace media
